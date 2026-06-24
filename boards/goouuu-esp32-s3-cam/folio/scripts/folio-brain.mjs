#!/usr/bin/env node
/**
 * folio-brain — ingest server, background workers, archive UI.
 */
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { bootstrapRuntime } from "./lib/bootstrap.mjs";
import { CFG } from "./lib/config.mjs";
import { createFolioServer, logServerStartup } from "./lib/http.mjs";
import { activeLocale, promptLanguageName } from "./lib/locale.mjs";
import { startInsightsLoop, startRetentionLoop, startProcessingLoop } from "./lib/services.mjs";

const UI_DIR = join(dirname(fileURLToPath(import.meta.url)), "ui");

function loadUi() {
  return {
    html: readFileSync(join(UI_DIR, "index.html"), "utf8").replaceAll("__PORT__", String(CFG.port)),
    css: readFileSync(join(UI_DIR, "app.css"), "utf8"),
    js: readFileSync(join(UI_DIR, "app.js"), "utf8"),
  };
}

function main() {
  const server = createFolioServer(loadUi());
  server.on("error", (err) => {
    if (err.code === "EADDRINUSE") {
      console.error(
        `[brain] port ${CFG.port} already in use — stop the other folio-brain ` +
          `(fuser -k ${CFG.port}/tcp) or change port in config`,
      );
      process.exit(1);
    }
    throw err;
  });
  server.listen(CFG.port, "0.0.0.0", async () => {
    logServerStartup();
    const boot = await bootstrapRuntime({ force: true });
    console.log(
      `frames: capture=${CFG.frameCaptureIntervalMs}ms · size=${CFG.frameSize}`,
    );
    console.log(
      `audio: chunk=${CFG.audioChunkMs}ms · retention=${CFG.audioRetentionDays}d`,
    );
    console.log(
      `lm: ${CFG.lmBaseUrl} · ${CFG.modelFast}` +
        `${CFG.modelDeep !== CFG.modelFast ? ` · insights=${CFG.modelDeep}` : ""}` +
        `${CFG.lmModelEmbed ? ` · embed=${CFG.lmModelEmbed}` : ""}`,
    );
    if (boot.stt?.ready) {
      console.log(`whisper: ${boot.stt.backend} · ${boot.stt.model} · ${boot.stt.device} (${boot.stt.bin})`);
    }
    console.log(`memory: ${boot.embeddings ? "embeddings" : "lexical"} · pipeline/insights auto`);
    for (const note of boot.notes ?? []) {
      console.log(`[auto] ${note}`);
    }
    console.log(`locale: ${activeLocale()} (${promptLanguageName()})`);

    if (CFG.pipelineEnabled) {
      startProcessingLoop();
    } else {
      console.log("[worker] disabled — set pipeline.enabled in config");
    }
  });

  if (CFG.insightsAuto) {
    startInsightsLoop();
  } else {
    console.log("[insights] auto disabled — set insights.auto in config");
  }

  startRetentionLoop();
}

main();
