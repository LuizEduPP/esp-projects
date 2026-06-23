#!/usr/bin/env node
/**
 * folio-brain — ingest server, processing loop, timeline UI.
 * ESP32 folio-node pushes audio/frames/events here. No responses to room.
 */
import { createServer } from "node:http";
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { networkInterfaces } from "node:os";
import { CFG } from "./lib/config.mjs";
import { getDigest, openDb, pendingCounts, timelineForDay } from "./lib/db.mjs";
import {
  ingestAudioChunk,
  ingestEvent,
  ingestFrame,
  runPendingQueueOnce,
  startProcessingLoop,
} from "./lib/pipeline.mjs";
import { runDigestForDay, startDigestLoop } from "./lib/digest_scheduler.mjs";
import { serveAudio, serveFrame } from "./lib/serve.mjs";
import { activeLocale, promptLanguageName, whisperLanguage } from "./lib/locale.mjs";
import { errMsg } from "./lib/util.mjs";

const today = () => new Date().toISOString().slice(0, 10);

function mediaIdFromPath(path, prefix) {
  const m = path.match(new RegExp(`^/api/${prefix}/(\\d+)$`));
  return m ? Number(m[1]) : null;
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

const UI_DIR = join(dirname(fileURLToPath(import.meta.url)), "ui");
const VIEW_HTML = readFileSync(join(UI_DIR, "index.html"), "utf8").replaceAll(
  "__PORT__",
  String(CFG.port),
);

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

function startServer() {
  openDb();

  const server = createServer(async (req, res) => {
    const path = req.url?.split("?")[0] ?? "/";
    const qs = new URL(req.url ?? "/", `http://127.0.0.1:${CFG.port}`).searchParams;

    try {
      if (path === "/" || path === "/index.html") {
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(VIEW_HTML);
        return;
      }

      if (path === "/ingest/audio" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"];
        if (!deviceId) {
          res.writeHead(400, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "X-Folio-Device-Id required" }));
          return;
        }
        const body = await readBody(req, 128 * 1024);
        const result = ingestAudioChunk(String(deviceId), body, req.headers["x-folio-meta"]);
        console.log(`[ingest] audio ${deviceId} seq=${result.id} bytes=${body.length} speech=${result.speech}`);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, ...result }));
        return;
      }

      if (path === "/ingest/frame" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"];
        if (!deviceId) {
          res.writeHead(400, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "X-Folio-Device-Id required" }));
          return;
        }
        const body = await readBody(req, 400 * 1024);
        const result = ingestFrame(String(deviceId), body, req.headers["x-folio-meta"]);
        console.log(`[ingest] frame ${deviceId} id=${result.id} bytes=${body.length} reason=${result.reason}`);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, ...result }));
        return;
      }

      if (path === "/ingest/event" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"] ?? "unknown";
        const body = JSON.parse((await readBody(req, 16 * 1024)).toString("utf8"));
        const result = ingestEvent(String(deviceId), body);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(result));
        return;
      }

      const frameId = mediaIdFromPath(path, "frame");
      if (frameId && req.method === "GET") {
        const db = openDb();
        const file = serveFrame(db, frameId);
        if (!file) {
          res.writeHead(404);
          res.end("not found");
          return;
        }
        res.writeHead(200, {
          "Content-Type": file.contentType,
          "Cache-Control": "private, max-age=3600",
        });
        res.end(file.body);
        return;
      }

      const audioId = mediaIdFromPath(path, "audio");
      if (audioId && req.method === "GET") {
        const db = openDb();
        const file = serveAudio(db, audioId);
        if (!file) {
          res.writeHead(404);
          res.end("not found");
          return;
        }
        res.writeHead(200, {
          "Content-Type": file.contentType,
          "Cache-Control": "private, max-age=3600",
        });
        res.end(file.body);
        return;
      }

      if (path === "/api/timeline") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ day, items: timelineForDay(db, day) }));
        return;
      }

      if (path === "/api/digest") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        const row = getDigest(db, day);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(row ?? { day, prose: null }));
        return;
      }

      if (path === "/api/digest/run" && req.method === "POST") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        const result = await runDigestForDay(db, day, { force: true });
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(
          JSON.stringify({
            ok: true,
            day,
            prose: result.prose ?? getDigest(db, day)?.prose ?? null,
            skipped: result.skipped ?? false,
          }),
        );
        return;
      }

      if (path === "/api/queue") {
        const db = openDb();
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(
          JSON.stringify({
            pending: pendingCounts(db),
            lm_gap_ms: CFG.frameCaptionIntervalMs,
            pipeline_interval_ms: CFG.pipelineIntervalMs,
          }),
        );
        return;
      }

      if (path === "/api/health") {
        const db = openDb();
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(
          JSON.stringify({
            ok: true,
            data_dir: CFG.dataDir,
            port: CFG.port,
            pending: pendingCounts(db),
            pipeline: CFG.pipelineEnabled,
          }),
        );
        return;
      }

      res.writeHead(404);
      res.end("not found");
    } catch (err) {
      res.writeHead(500, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: errMsg(err) }));
    }
  });

  server.listen(CFG.port, "0.0.0.0", () => {
    console.log(`folio-brain listening on 0.0.0.0:${CFG.port}`);
    console.log(`  local   http://127.0.0.1:${CFG.port}`);
    for (const url of lanUrls(CFG.port)) {
      console.log(`  lan     ${url}  ← set FOLIO_BRAIN_URL on ESP`);
    }
    console.log(`data: ${CFG.dataDir}`);
    console.log(`models: fast=${CFG.modelFast} deep=${CFG.modelDeep}`);
    console.log(
      `locale: ${activeLocale()} (${promptLanguageName()}) · whisper=${whisperLanguage()}`,
    );
  });
}

function main() {
  startServer();
  if (CFG.pipelineEnabled) {
    startProcessingLoop();
  } else {
    console.log("[pipeline] disabled (FOLIO_PIPELINE=0) — run: node scripts/folio-process.mjs");
  }
  if (CFG.digestAuto) {
    startDigestLoop();
  } else {
    console.log("[digest] auto disabled (FOLIO_DIGEST_AUTO=0)");
  }
}

main();
