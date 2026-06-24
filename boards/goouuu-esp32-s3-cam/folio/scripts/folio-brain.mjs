#!/usr/bin/env node
/**
 * folio-brain — ingest server, background workers, archive UI.
 */
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CFG, isCudaAvailable } from "./lib/config.mjs";
import { createFolioServer, logServerStartup } from "./lib/http.mjs";
import { activeLocale, promptLanguageName, whisperLanguageCode } from "./lib/locale.mjs";
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
  server.listen(CFG.port, "0.0.0.0", () => {
    logServerStartup();
    console.log(
      `frames: capture=${CFG.frameCaptureIntervalMs}ms caption=${CFG.frameCaptionIntervalMs}ms ` +
        `size=${CFG.frameSize} jpegQ=${CFG.frameJpegQuality}`,
    );
    console.log(
      `audio: chunk=${CFG.audioChunkMs}ms rate=${CFG.audioSampleRate} ` +
        `speech≥${CFG.speechEnergyThreshold} retention=${CFG.audioRetentionDays}d`,
    );
    console.log(
      `lm studio: ${CFG.lmBaseUrl} · vision=${CFG.modelFast}` +
        `${CFG.modelDeep !== CFG.modelFast ? ` insights=${CFG.modelDeep}` : ""}` +
        `${CFG.lmModelEmbed ? ` embed=${CFG.lmModelEmbed}` : ""}` +
        `${CFG.lmModelRerank ? ` rerank=${CFG.lmModelRerank}` : ""}`,
    );
    const whisperLang = CFG.whisperLanguage ? whisperLanguageCode() : "auto";
    console.log(
      `whisper (local): ${CFG.whisperBin} model=${CFG.whisperModel} device=${CFG.whisperDevice} ` +
        `lang=${whisperLang} cuda=${isCudaAvailable() ? "yes" : "no"}`,
    );
    console.log("[config] hot reload: LM Studio/Whisper/locale apply on save — restart only for port/dataDir");
  });

  if (CFG.pipelineEnabled) {
    startProcessingLoop();
  } else {
    console.log("[worker] disabled — set pipeline.enabled in config");
  }

  if (CFG.insightsAuto) {
    startInsightsLoop();
  } else {
    console.log("[insights] auto disabled — set insights.auto in config");
  }

  startRetentionLoop();
}

main();
