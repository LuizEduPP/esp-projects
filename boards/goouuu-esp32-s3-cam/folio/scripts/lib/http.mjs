import { createServer } from "node:http";
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { CFG, nodeConfigPayload, publicConfig, updateConfig } from "./config.mjs";
import { fetchOpenAiModels } from "./llm.mjs";
import { listLanUrls } from "./network.mjs";
import {
  getInsightsForApi, ingestAudioChunk, ingestEvent, ingestFrame, needsInsightsRefresh,
  runDayInsights, runPendingQueueOnce,
} from "./services.mjs";
import { reindexAllMemories, retrieveMemories } from "./memory.mjs";
import { sttCapability } from "./stt-capability.mjs";
import { errMsg, pcmToWav, sendBytes, sendJson, today } from "./util.mjs";
import {
  getAudioChunk, getFrame, listDevices, listEntities, memoryChunkCount, openDb,
  pendingCounts, timelineForDay, touchDevice,
} from "./db.mjs";
import { timelineWithGroups } from "./present.mjs";

const UI_MIME = {
  "/ui/app.css": "text/css; charset=utf-8",
  "/ui/app.js": "application/javascript; charset=utf-8",
};

let quietSkipCount = 0;
let lastQuietLogAt = 0;

function logAudioIngest(deviceId, body, result) {
  if (!result.skipped) {
    console.log(
      `[ingest] audio ${deviceId} id=${result.id} bytes=${body.length} energy=${result.energy?.toFixed(4)} → stt queue`,
    );
    return;
  }

  quietSkipCount++;
  const now = Date.now();
  if (quietSkipCount <= 3 || now - lastQuietLogAt >= 30_000) {
    console.log(
      `[ingest] audio ${deviceId} skip ${result.skipped} energy=${result.energy?.toFixed(4)} (no stt)`,
    );
    if (quietSkipCount > 3) {
      quietSkipCount = 0;
    }
    lastQuietLogAt = now;
  }
}

async function readBody(req, maxBytes = CFG.httpBodyMaxBytes) {
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

function noteDevice(req) {
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

function assertUnderDataDir(filePath) {
  const base = resolve(CFG.dataDir);
  const abs = resolve(filePath);
  if (abs !== base && !abs.startsWith(`${base}/`)) {
    throw new Error("path outside data directory");
  }
  return abs;
}

function serveFrame(db, frameId) {
  const frame = getFrame(db, Number(frameId));
  if (!frame) {
    return null;
  }
  const body = readFileSync(assertUnderDataDir(frame.path));
  return { body, contentType: "image/jpeg" };
}

function serveAudio(db, chunkId) {
  const chunk = getAudioChunk(db, Number(chunkId));
  if (!chunk) {
    return null;
  }
  if (!chunk.path) {
    return { gone: true };
  }
  const pcm = readFileSync(assertUnderDataDir(chunk.path));
  return { body: pcmToWav(pcm, CFG.audioSampleRate), contentType: "audio/wav" };
}

export function createFolioServer(ui) {
  openDb();

  return createServer(async (req, res) => {
    const path = req.url?.split("?")[0] ?? "/";
    const qs = new URL(req.url ?? "/", `http://127.0.0.1:${CFG.port}`).searchParams;

    try {
      if (path === "/" || path === "/index.html") {
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(ui.html);
        return;
      }

      if (UI_MIME[path]) {
        const asset = path === "/ui/app.css" ? ui.css : ui.js;
        res.writeHead(200, { "Content-Type": UI_MIME[path] });
        res.end(asset ?? "");
        return;
      }

      if (path === "/ingest/audio" && req.method === "POST") {
        const deviceId = deviceIdFromReq(req);
        if (!deviceId) {
          sendJson(res, 400, { error: "X-Folio-Device-Id required" });
          return;
        }
        noteDevice(req);
        const body = await readBody(req, CFG.httpIngestAudioMaxBytes);
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
        const body = await readBody(req, CFG.httpIngestFrameMaxBytes);
        const result = ingestFrame(deviceId, body, req.headers["x-folio-meta"]);
        console.log(`[ingest] frame ${deviceId} id=${result.id} bytes=${body.length} reason=${result.reason}`);
        sendJson(res, 200, { ok: true, ...result });
        return;
      }

      if (path === "/ingest/event" && req.method === "POST") {
        const deviceId = deviceIdFromReq(req);
        if (!deviceId) {
          sendJson(res, 400, { error: "X-Folio-Device-Id required" });
          return;
        }
        noteDevice(req);
        const body = JSON.parse((await readBody(req, CFG.httpIngestEventMaxBytes)).toString("utf8"));
        sendJson(res, 200, ingestEvent(deviceId, body));
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
        if (file.gone) {
          sendJson(res, 410, { error: "PCM expired — transcript may still be in timeline" });
          return;
        }
        sendBytes(res, 200, file.body, file.contentType);
        return;
      }

      if (path === "/api/timeline") {
        const day = qs.get("day") ?? today();
        const items = timelineForDay(openDb(), day);
        const grouped = timelineWithGroups(items);
        sendJson(res, 200, { day, items, ...grouped });
        return;
      }

      if (path === "/api/insights") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        sendJson(res, 200, {
          ...getInsightsForApi(db, day),
          status: { ...needsInsightsRefresh(db, day), interval_ms: CFG.insightsIntervalMs },
        });
        return;
      }

      if (path === "/api/insights/run" && req.method === "POST") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        const result = await runDayInsights(db, day, { force: true });
        sendJson(res, 200, { ok: true, ...getInsightsForApi(db, day), result });
        return;
      }

      if (path === "/api/entities") {
        sendJson(res, 200, { entities: listEntities(openDb()) });
        return;
      }

      if (path === "/api/process" && req.method === "POST") {
        const { audio, frames } = await runPendingQueueOnce({ bypassFrameGap: true });
        sendJson(res, 200, {
          ok: true,
          pending: pendingCounts(openDb()),
          audio,
          frames,
        });
        return;
      }

      if (path === "/api/memory/reindex" && req.method === "POST") {
        const db = openDb();
        const before = memoryChunkCount(db);
        const result = await reindexAllMemories(db);
        sendJson(res, 200, { ok: true, before, after: memoryChunkCount(db), ...result });
        return;
      }

      if (path === "/api/config" && req.method === "GET") {
        sendJson(res, 200, publicConfig());
        return;
      }

      if (path === "/api/openai/models" || path === "/api/lm/models") {
        sendJson(res, 200, await fetchOpenAiModels());
        return;
      }

      if (path === "/api/config" && req.method === "PUT") {
        const body = JSON.parse((await readBody(req, CFG.httpConfigPatchMaxBytes)).toString("utf8"));
        const result = updateConfig(body);
        sendJson(res, 200, result);
        return;
      }

      if (path === "/api/node/config" && req.method === "GET") {
        const deviceId = deviceIdFromReq(req);
        if (deviceId) {
          touchDevice(openDb(), deviceId);
        }
        const clientIp = req.socket?.remoteAddress ?? null;
        sendJson(res, 200, nodeConfigPayload(clientIp));
        return;
      }

      if (path === "/api/devices") {
        const db = openDb();
        const payload = nodeConfigPayload();
        sendJson(res, 200, {
          brain_config_version: payload.version,
          brain_url: payload.brainUrl,
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
        sendJson(res, 200, { query: q, day, chunks: memoryChunkCount(db), hits });
        return;
      }

      if (path === "/api/health") {
        const stt = sttCapability();
        sendJson(res, 200, {
          ok: true,
          today: today(),
          timezone: Intl.DateTimeFormat().resolvedOptions().timeZone,
          data_dir: CFG.dataDir,
          port: CFG.port,
          pending: pendingCounts(openDb()),
          pipeline: CFG.pipelineEnabled,
          insights: CFG.insightsAuto,
          memory_chunks: memoryChunkCount(openDb()),
          stt: stt.ready
            ? { ready: true, backend: stt.backend, model: stt.model }
            : { ready: false },
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
  return listLanUrls(port);
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