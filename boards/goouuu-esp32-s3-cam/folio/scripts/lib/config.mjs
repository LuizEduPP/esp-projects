import { homedir } from "node:os";
import { join } from "node:path";

export const CFG = {
  port: Number(process.env.FOLIO_PORT ?? "8770"),
  dataDir: process.env.FOLIO_DATA_DIR ?? join(homedir(), ".folio"),
  lmUrl: process.env.LM_URL ?? "http://127.0.0.1:1234/v1/chat/completions",
  modelFast: process.env.FOLIO_MODEL_FAST ?? "mistralai/ministral-3-3b",
  modelDeep: process.env.FOLIO_MODEL_DEEP ?? process.env.FOLIO_MODEL_FAST ?? "mistralai/ministral-3-3b",
  whisperBin: process.env.FOLIO_WHISPER_BIN ?? "whisper",
  whisperModel: process.env.FOLIO_WHISPER_MODEL ?? "base",
  episodeGapMin: Number(process.env.FOLIO_EPISODE_GAP_MIN ?? "12"),
  frameCaptionIntervalMs: Number(process.env.FOLIO_FRAME_CAPTION_MS ?? "0"),
  audioRetentionDays: Number(process.env.FOLIO_AUDIO_RETENTION_DAYS ?? "7"),
  defaultLocale: process.env.FOLIO_LOCALE ?? "pt-BR",
};

export const PATHS = {
  db: () => join(CFG.dataDir, "folio.db"),
  audioDir: (day) => join(CFG.dataDir, "audio", day),
  frameDir: (day) => join(CFG.dataDir, "frames", day),
  speakerDir: () => join(CFG.dataDir, "speakers"),
  digestDir: () => join(CFG.dataDir, "digests"),
};
