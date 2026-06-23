#!/usr/bin/env node
/**
 * folio-brain — ingest server, background workers, timeline UI.
 */
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CFG } from "./lib/config.mjs";
import { startDigestLoop } from "./lib/digest.mjs";
import { createFolioServer, logServerStartup } from "./lib/http.mjs";
import { activeLocale, promptLanguageName, whisperLanguageCode } from "./lib/locale.mjs";
import { startRetentionLoop, startProcessingLoop } from "./lib/worker.mjs";

const UI_DIR = join(dirname(fileURLToPath(import.meta.url)), "ui");
const viewHtml = readFileSync(join(UI_DIR, "index.html"), "utf8").replaceAll(
  "__PORT__",
  String(CFG.port),
);

function main() {
  const server = createFolioServer(viewHtml);
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
    console.log(`models: ${CFG.modelFast} · locale: ${activeLocale()} (${promptLanguageName()})`);
    console.log(
      `whisper: ${CFG.whisperBin} model=${CFG.whisperModel} device=${CFG.whisperDevice} ` +
        `lang=${whisperLanguageCode()}`,
    );
  });

  if (CFG.pipelineEnabled) {
    startProcessingLoop();
  } else {
    console.log("[worker] disabled (FOLIO_PIPELINE=0) — set pipeline.enabled in config");
  }

  if (CFG.digestAuto) {
    startDigestLoop();
  } else {
    console.log("[digest] auto disabled (FOLIO_DIGEST_AUTO=0)");
  }

  startRetentionLoop();
}

main();
