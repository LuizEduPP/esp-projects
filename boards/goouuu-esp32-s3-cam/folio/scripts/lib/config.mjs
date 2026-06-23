import { homedir } from "node:os";
import { join } from "node:path";
import { execSync } from "node:child_process";
import { loadFileConfig, makeResolver } from "./load-config.mjs";

function resolveWhisperBin(fileVal, envVal) {
  if (envVal) {
    return envVal;
  }
  if (fileVal && fileVal !== "whisper") {
    return fileVal;
  }
  try {
    return execSync("which whisper", { encoding: "utf8", env: process.env }).trim() || "whisper";
  } catch {
    return fileVal || "whisper";
  }
}

const { path: configPath, data: file } = loadFileConfig();
const { num, str, bool } = makeResolver(file);

const dataDir =
  str("dataDir", "FOLIO_DATA_DIR", "") || join(homedir(), ".folio");

export const CFG = {
  configPath,
  port: num("port", "FOLIO_PORT", 8770),
  dataDir,

  lmUrl: str("lm.url", "LM_URL", "http://127.0.0.1:1234/v1/chat/completions"),
  modelFast: str("lm.modelFast", "FOLIO_MODEL_FAST", "mistralai/ministral-3-3b"),
  modelDeep: str(
    "lm.modelDeep",
    "FOLIO_MODEL_DEEP",
    str("lm.modelFast", "FOLIO_MODEL_FAST", "mistralai/ministral-3-3b"),
  ),

  /** ESP: interval between JPEG captures (documented; set in platformio.ini). */
  frameCaptureIntervalMs: num("frames.captureIntervalMs", "FOLIO_FRAME_INTERVAL_MS", 60000),
  frameCaptionIntervalMs: num("frames.captionIntervalMs", "FOLIO_FRAME_CAPTION_MS", 60000),
  frameCaptionMaxTokens: num("frames.captionMaxTokens", "FOLIO_FRAME_CAPTION_MAX_TOKENS", 220),
  frameCaptionTemperature: num(
    "frames.captionTemperature",
    "FOLIO_FRAME_CAPTION_TEMP",
    0.05,
  ),
  pipelineFrameBatch: num("frames.pipelineBatch", "FOLIO_PIPELINE_FRAME_BATCH", 1),
  frameJpegQuality: num("frames.jpegQuality", "FOLIO_JPEG_QUALITY", 12),
  frameSize: str("frames.size", "FOLIO_FRAME_SIZE", "QVGA"),

  audioChunkMs: num("audio.chunkMs", "FOLIO_AUDIO_CHUNK_MS", 1000),
  audioSampleRate: num("audio.sampleRate", "FOLIO_AUDIO_SAMPLE_RATE", 16000),
  speechEnergyThreshold: num(
    "audio.speechEnergyThreshold",
    "FOLIO_SPEECH_ENERGY",
    0.008,
  ),
  whisperBin: resolveWhisperBin(
    str("audio.whisperBin", "FOLIO_WHISPER_BIN", "whisper"),
    process.env.FOLIO_WHISPER_BIN,
  ),
  whisperModel: str("audio.whisperModel", "FOLIO_WHISPER_MODEL", "base"),
  whisperDevice: str("audio.whisperDevice", "FOLIO_WHISPER_DEVICE", "cpu"),
  whisperTimeoutMs: num("audio.whisperTimeoutMs", "FOLIO_WHISPER_TIMEOUT_MS", 120000),
  whisperLanguage: str("audio.whisperLanguage", "FOLIO_WHISPER_LANGUAGE", "") || null,
  pipelineAudioBatch: num("audio.pipelineBatch", "FOLIO_PIPELINE_AUDIO_BATCH", 4),
  /** Save PCM files for below-threshold (quiet) chunks — default off. */
  audioStoreQuiet: bool("audio.storeQuiet", "FOLIO_AUDIO_STORE_QUIET", false),
  audioRetentionDays: num("audio.retentionDays", "FOLIO_AUDIO_RETENTION_DAYS", 7),

  pipelineIntervalMs: num("pipeline.intervalMs", "FOLIO_PIPELINE_INTERVAL_MS", 30000),
  pipelineEnabled: bool("pipeline.enabled", "FOLIO_PIPELINE", true),

  digestIntervalMs: num("digest.intervalMs", "FOLIO_DIGEST_INTERVAL_MS", 1800000),
  digestAuto: bool("digest.auto", "FOLIO_DIGEST_AUTO", true),

  episodeGapMin: num("episodes.gapMin", "FOLIO_EPISODE_GAP_MIN", 12),

  defaultLocale: str("locale", "FOLIO_LOCALE", "pt-BR"),

  /** Documented for ESP (platformio.ini build_flags). */
  nodeWifiRetryMs: num("node.wifiRetryMs", "FOLIO_WIFI_RETRY_MS", 5000),
  nodePushBackoffMaxMs: num("node.pushBackoffMaxMs", "FOLIO_PUSH_BACKOFF_MAX_MS", 30000),
  nodeStatusIntervalMs: num("node.statusIntervalMs", "FOLIO_STATUS_INTERVAL_MS", 15000),
};

export const PATHS = {
  db: () => join(CFG.dataDir, "folio.db"),
  audioDir: (day) => join(CFG.dataDir, "audio", day),
  frameDir: (day) => join(CFG.dataDir, "frames", day),
  speakerDir: () => join(CFG.dataDir, "speakers"),
  digestDir: () => join(CFG.dataDir, "digests"),
};

/** Public snapshot for /api/config (no secrets). */
export function publicConfig() {
  return {
    configPath: CFG.configPath,
    locale: CFG.defaultLocale,
    port: CFG.port,
    dataDir: CFG.dataDir,
    lm: { modelFast: CFG.modelFast, modelDeep: CFG.modelDeep },
    frames: {
      captureIntervalMs: CFG.frameCaptureIntervalMs,
      captionIntervalMs: CFG.frameCaptionIntervalMs,
      captionMaxTokens: CFG.frameCaptionMaxTokens,
      pipelineBatch: CFG.pipelineFrameBatch,
      jpegQuality: CFG.frameJpegQuality,
      size: CFG.frameSize,
    },
    audio: {
      chunkMs: CFG.audioChunkMs,
      sampleRate: CFG.audioSampleRate,
      speechEnergyThreshold: CFG.speechEnergyThreshold,
      whisperModel: CFG.whisperModel,
      whisperBin: CFG.whisperBin,
      whisperDevice: CFG.whisperDevice,
      pipelineBatch: CFG.pipelineAudioBatch,
      storeQuiet: CFG.audioStoreQuiet,
      retentionDays: CFG.audioRetentionDays,
    },
    pipeline: {
      enabled: CFG.pipelineEnabled,
      intervalMs: CFG.pipelineIntervalMs,
    },
    digest: { auto: CFG.digestAuto, intervalMs: CFG.digestIntervalMs },
    episodes: { gapMin: CFG.episodeGapMin },
  };
}
