#!/usr/bin/env node
/**
 * folio-brain — ingest server, background workers, timeline UI.
 */
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CFG } from "./lib/config.mjs";
import { startDigestLoop } from "./lib/digest/scheduler.mjs";
import { createFolioServer, logServerStartup } from "./lib/http.mjs";
import { activeLocale, promptLanguageName, whisperLanguageCode } from "./lib/locale.mjs";
import { startRetentionLoop } from "./lib/retention.mjs";
import { startProcessingLoop } from "./lib/worker.mjs";

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
    console.log(`models: fast=${CFG.modelFast} deep=${CFG.modelDeep}`);
    console.log(
      `locale: ${activeLocale()} (${promptLanguageName()}) · whisper=${whisperLanguageCode()} @ ${CFG.whisperBin}`,
    );
  });

  if (CFG.pipelineEnabled) {
    startProcessingLoop();
  } else {
    console.log("[worker] disabled (FOLIO_PIPELINE=0) — run: yarn folio process");
  }

  if (CFG.digestAuto) {
    startDigestLoop();
  } else {
    console.log("[digest] auto disabled (FOLIO_DIGEST_AUTO=0)");
  }

  startRetentionLoop();
}

main();
