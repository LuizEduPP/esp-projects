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
  /** Min gap between LM vision calls (ESP sends ~1 frame/min). */
  frameCaptionIntervalMs: Number(process.env.FOLIO_FRAME_CAPTION_MS ?? "60000"),
  /** How often the pending-queue worker wakes up. */
  pipelineIntervalMs: Number(process.env.FOLIO_PIPELINE_INTERVAL_MS ?? "30000"),
  pipelineAudioBatch: Number(process.env.FOLIO_PIPELINE_AUDIO_BATCH ?? "2"),
  pipelineFrameBatch: Number(process.env.FOLIO_PIPELINE_FRAME_BATCH ?? "1"),
  /** Set FOLIO_PIPELINE=0 to ingest-only; run `yarn brain:process` manually. */
  pipelineEnabled: process.env.FOLIO_PIPELINE !== "0",
  /** Auto digest interval — runs when new witness data since last digest. */
  digestIntervalMs: Number(process.env.FOLIO_DIGEST_INTERVAL_MS ?? "1800000"),
  digestAuto: process.env.FOLIO_DIGEST_AUTO !== "0",
  /** BCP-47 locale — controls LM prompt language and digest prose (FOLIO_LOCALE). */
  defaultLocale: process.env.FOLIO_LOCALE ?? "pt-BR",
  /** Whisper --language override; defaults from FOLIO_LOCALE. */
  whisperLanguage: process.env.FOLIO_WHISPER_LANGUAGE ?? null,
  audioRetentionDays: Number(process.env.FOLIO_AUDIO_RETENTION_DAYS ?? "7"),
};

export const PATHS = {
  db: () => join(CFG.dataDir, "folio.db"),
  audioDir: (day) => join(CFG.dataDir, "audio", day),
  frameDir: (day) => join(CFG.dataDir, "frames", day),
  speakerDir: () => join(CFG.dataDir, "speakers"),
  digestDir: () => join(CFG.dataDir, "digests"),
};
