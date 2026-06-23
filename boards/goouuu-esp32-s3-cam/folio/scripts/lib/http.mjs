import { createServer } from "node:http";
import { networkInterfaces } from "node:os";
import { CFG, nodeConfigPayload, publicConfig, updateConfig } from "./config.mjs";
import { runDigestForDay } from "./digest/scheduler.mjs";
import {
  getDigest,
  listDevices,
  openDb,
  memoryChunkCount,
  pendingCounts,
  profileFacts,
  setBrainConfigVersion,
  timelineForDay,
  touchDevice,
} from "./db.mjs";
import { ingestAudioChunk, ingestFrame, ingestEvent } from "./ingest.mjs";
import { serveAudio, serveFrame } from "./serve.mjs";
import { retrieveMemories } from "./memory/retrieve.mjs";
import { errMsg, sendBytes, sendJson, today } from "./util.mjs";

let quietSkipCount = 0;
let lastQuietLogAt = 0;

function logAudioIngest(deviceId, body, result) {
  if (!result.skipped) {
    console.log(
      `[ingest] audio ${deviceId} id=${result.id} bytes=${body.length} energy=${result.energy?.toFixed(4)}`,
    );
    return;
  }

  quietSkipCount++;
  const now = Date.now();
  if (now - lastQuietLogAt >= 60_000) {
    console.log(`[ingest] audio ${deviceId} skipped ${quietSkipCount} empty/quiet chunks (last 60s)`);
    quietSkipCount = 0;
    lastQuietLogAt = now;
  }
}

async function readBody(req, maxBytes = 512 * 1024) {
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size > maxBytes) {
      throw new Error("body too large");
    }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}

function mediaIdFromPath(path, prefix) {
  const m = path.match(new RegExp(`^/api/${prefix}/(\\d+)$`));
  return m ? Number(m[1]) : null;
}

function deviceIdFromReq(req) {
  return String(req.headers["x-folio-device-id"] ?? "").trim() || null;
}

function noteDevice(req, res) {
  const deviceId = deviceIdFromReq(req);
  if (!deviceId) {
    return null;
  }
  const db = openDb();
  const applied = req.headers["x-folio-config-version"];
  if (applied) {
    touchDevice(db, deviceId, { configVersion: String(applied) });
  } else {
    touchDevice(db, deviceId);
  }
  return deviceId;
}

export function createFolioServer(viewHtml) {
  openDb();

  return createServer(async (req, res) => {
    const path = req.url?.split("?")[0] ?? "/";
    const qs = new URL(req.url ?? "/", `http://127.0.0.1:${CFG.port}`).searchParams;

    try {
      if (path === "/" || path === "/index.html") {
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(viewHtml);
        return;
      }

      if (path === "/ingest/audio" && req.method === "POST") {
        const deviceId = deviceIdFromReq(req);
        if (!deviceId) {
          sendJson(res, 400, { error: "X-Folio-Device-Id required" });
          return;
        }
        noteDevice(req);
        const body = await readBody(req, 128 * 1024);
        const result = ingestAudioChunk(deviceId, body, req.headers["x-folio-meta"]);
        logAudioIngest(deviceId, body, result);
        sendJson(res, 200, { ok: true, ...result });
        return;
      }

      if (path === "/ingest/frame" && req.method === "POST") {
        const deviceId = deviceIdFromReq(req);
        if (!deviceId) {
          sendJson(res, 400, { error: "X-Folio-Device-Id required" });
          return;
        }
        noteDevice(req);
        const body = await readBody(req, 400 * 1024);
        const result = ingestFrame(deviceId, body, req.headers["x-folio-meta"]);
        console.log(`[ingest] frame ${deviceId} id=${result.id} bytes=${body.length} reason=${result.reason}`);
        sendJson(res, 200, { ok: true, ...result });
        return;
      }

      if (path === "/ingest/event" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"] ?? "unknown";
        noteDevice(req);
        const body = JSON.parse((await readBody(req, 16 * 1024)).toString("utf8"));
        sendJson(res, 200, ingestEvent(String(deviceId), body));
        return;
      }

      const frameId = mediaIdFromPath(path, "frame");
      if (frameId && req.method === "GET") {
        const file = serveFrame(openDb(), frameId);
        if (!file) {
          res.writeHead(404);
          res.end("not found");
          return;
        }
        sendBytes(res, 200, file.body, file.contentType);
        return;
      }

      const audioId = mediaIdFromPath(path, "audio");
      if (audioId && req.method === "GET") {
        const file = serveAudio(openDb(), audioId);
        if (!file) {
          res.writeHead(404);
          res.end("not found");
          return;
        }
        sendBytes(res, 200, file.body, file.contentType);
        return;
      }

      if (path === "/api/timeline") {
        const day = qs.get("day") ?? today();
        sendJson(res, 200, { day, items: timelineForDay(openDb(), day) });
        return;
      }

      if (path === "/api/digest") {
        const day = qs.get("day") ?? today();
        const row = getDigest(openDb(), day);
        sendJson(res, 200, row ?? { day, prose: null });
        return;
      }

      if (path === "/api/digest/run" && req.method === "POST") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        const result = await runDigestForDay(db, day, { force: true });
        sendJson(res, 200, {
          ok: true,
          day,
          prose: result.prose ?? getDigest(db, day)?.prose ?? null,
          skipped: result.skipped ?? false,
        });
        return;
      }

      if (path === "/api/config" && req.method === "GET") {
        sendJson(res, 200, publicConfig());
        return;
      }

      if (path === "/api/config" && req.method === "PUT") {
        const body = JSON.parse((await readBody(req, 64 * 1024)).toString("utf8"));
        const result = updateConfig(body);
        setBrainConfigVersion(openDb(), result.version);
        sendJson(res, 200, result);
        return;
      }

      if (path === "/api/node/config" && req.method === "GET") {
        const deviceId = deviceIdFromReq(req);
        if (deviceId) {
          touchDevice(openDb(), deviceId);
        }
        const payload = nodeConfigPayload();
        setBrainConfigVersion(openDb(), payload.version);
        sendJson(res, 200, payload);
        return;
      }

      if (path === "/api/devices") {
        const db = openDb();
        const brainVersion = nodeConfigPayload().version;
        sendJson(res, 200, {
          brain_config_version: brainVersion,
          devices: listDevices(db),
        });
        return;
      }

      if (path === "/api/queue") {
        sendJson(res, 200, {
          pending: pendingCounts(openDb()),
          lm_gap_ms: CFG.frameCaptionIntervalMs,
          pipeline_interval_ms: CFG.pipelineIntervalMs,
        });
        return;
      }

      if (path === "/api/memory") {
        const q = qs.get("q") ?? "";
        const day = qs.get("day") ?? today();
        const db = openDb();
        const hits = q.trim() ? await retrieveMemories(db, q, { day }) : [];
        sendJson(res, 200, {
          query: q,
          day,
          chunks: memoryChunkCount(db),
          profile: profileFacts(db),
          hits,
        });
        return;
      }

      if (path === "/api/health") {
        sendJson(res, 200, {
          ok: true,
          data_dir: CFG.dataDir,
          port: CFG.port,
          pending: pendingCounts(openDb()),
          pipeline: CFG.pipelineEnabled,
          memory_chunks: memoryChunkCount(openDb()),
        });
        return;
      }

      res.writeHead(404);
      res.end("not found");
    } catch (err) {
      sendJson(res, 500, { error: errMsg(err) });
    }
  });
}

function lanUrls(port) {
  const urls = [];
  for (const ifaces of Object.values(networkInterfaces())) {
    for (const iface of ifaces ?? []) {
      if (iface.family === "IPv4" && !iface.internal) {
        urls.push(`http://${iface.address}:${port}`);
      }
    }
  }
  return urls;
}

export function logServerStartup() {
  console.log(`folio-brain listening on 0.0.0.0:${CFG.port}`);
  console.log(`  local   http://127.0.0.1:${CFG.port}`);
  for (const url of lanUrls(CFG.port)) {
    console.log(`  lan     ${url}  ← set FOLIO_BRAIN_URL on ESP`);
  }
  console.log(`data: ${CFG.dataDir}`);
  if (CFG.configPath) {
    console.log(`config: ${CFG.configPath}`);
  } else {
    console.log("config: (defaults) copy folio.config.example.json → ~/.folio/config.json");
  }
}
