import { execSync } from "node:child_process";
import { homedir } from "node:os";
import { join } from "node:path";
import { getConfigPath, getFileData, initConfigStore } from "./config-store.mjs";

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

function num(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return Number(process.env[envKey]);
  }
  return fileVal !== undefined ? Number(fileVal) : fallback;
}

function str(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return process.env[envKey];
  }
  return fileVal !== undefined ? String(fileVal) : fallback;
}

function bool(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return process.env[envKey] !== "0" && process.env[envKey] !== "false";
  }
  return fileVal !== undefined ? Boolean(fileVal) : fallback;
}

function getPath(obj, dotPath) {
  return dotPath.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

export function buildCfgFromFile(file = getFileData()) {
  const dataDir = str(getPath(file, "dataDir"), "FOLIO_DATA_DIR", "") || join(homedir(), ".folio");
  return {
    configPath: getConfigPath(),
    port: num(getPath(file, "port"), "FOLIO_PORT", 8770),
    dataDir,

    lmUrl: str(getPath(file, "lm.url"), "LM_URL", "http://127.0.0.1:1234/v1/chat/completions"),
    modelFast: str(getPath(file, "lm.modelFast"), "FOLIO_MODEL_FAST", "mistralai/ministral-3-3b"),
    modelDeep: str(
      getPath(file, "lm.modelDeep"),
      "FOLIO_MODEL_DEEP",
      str(getPath(file, "lm.modelFast"), "FOLIO_MODEL_FAST", "mistralai/ministral-3-3b"),
    ),

    frameCaptureIntervalMs: num(
      getPath(file, "frames.captureIntervalMs"),
      "FOLIO_FRAME_INTERVAL_MS",
      60000,
    ),
    frameCaptionIntervalMs: num(
      getPath(file, "frames.captionIntervalMs"),
      "FOLIO_FRAME_CAPTION_MS",
      60000,
    ),
    frameCaptionMaxTokens: num(
      getPath(file, "frames.captionMaxTokens"),
      "FOLIO_FRAME_CAPTION_MAX_TOKENS",
      220,
    ),
    frameCaptionTemperature: num(
      getPath(file, "frames.captionTemperature"),
      "FOLIO_FRAME_CAPTION_TEMP",
      0.05,
    ),
    pipelineFrameBatch: num(getPath(file, "frames.pipelineBatch"), "FOLIO_PIPELINE_FRAME_BATCH", 1),
    frameJpegQuality: num(getPath(file, "frames.jpegQuality"), "FOLIO_JPEG_QUALITY", 12),
    frameSize: str(getPath(file, "frames.size"), "FOLIO_FRAME_SIZE", "QVGA"),

    audioChunkMs: num(getPath(file, "audio.chunkMs"), "FOLIO_AUDIO_CHUNK_MS", 1000),
    audioSampleRate: num(getPath(file, "audio.sampleRate"), "FOLIO_AUDIO_SAMPLE_RATE", 16000),
    speechEnergyThreshold: num(
      getPath(file, "audio.speechEnergyThreshold"),
      "FOLIO_SPEECH_ENERGY",
      0.008,
    ),
    whisperBin: resolveWhisperBin(
      str(getPath(file, "audio.whisperBin"), "FOLIO_WHISPER_BIN", "whisper"),
      process.env.FOLIO_WHISPER_BIN,
    ),
    whisperModel: str(getPath(file, "audio.whisperModel"), "FOLIO_WHISPER_MODEL", "base"),
    whisperDevice: str(getPath(file, "audio.whisperDevice"), "FOLIO_WHISPER_DEVICE", "cpu"),
    whisperTimeoutMs: num(
      getPath(file, "audio.whisperTimeoutMs"),
      "FOLIO_WHISPER_TIMEOUT_MS",
      120000,
    ),
    whisperLanguage: str(getPath(file, "audio.whisperLanguage"), "FOLIO_WHISPER_LANGUAGE", "") || null,
    pipelineAudioBatch: num(getPath(file, "audio.pipelineBatch"), "FOLIO_PIPELINE_AUDIO_BATCH", 4),
    audioRetentionDays: num(getPath(file, "audio.retentionDays"), "FOLIO_AUDIO_RETENTION_DAYS", 7),

    pipelineIntervalMs: num(getPath(file, "pipeline.intervalMs"), "FOLIO_PIPELINE_INTERVAL_MS", 30000),
    pipelineEnabled: bool(getPath(file, "pipeline.enabled"), "FOLIO_PIPELINE", true),

    digestIntervalMs: num(getPath(file, "digest.intervalMs"), "FOLIO_DIGEST_INTERVAL_MS", 1800000),
    digestAuto: bool(getPath(file, "digest.auto"), "FOLIO_DIGEST_AUTO", true),

    episodeGapMin: num(getPath(file, "episodes.gapMin"), "FOLIO_EPISODE_GAP_MIN", 12),

    memoryEnabled: bool(getPath(file, "memory.enabled"), "FOLIO_MEMORY", true),
    memoryLookbackDays: num(getPath(file, "memory.lookbackDays"), "FOLIO_MEMORY_LOOKBACK_DAYS", 90),
    memoryRetrieveLimit: num(getPath(file, "memory.retrieveLimit"), "FOLIO_MEMORY_RETRIEVE", 14),
    memoryMinScore: num(getPath(file, "memory.minScore"), "FOLIO_MEMORY_MIN_SCORE", 0.08),
    memoryUseEmbeddings: bool(getPath(file, "memory.useEmbeddings"), "FOLIO_MEMORY_EMBEDDINGS", false),
    memoryEmbeddingModel:
      str(getPath(file, "memory.embeddingModel"), "FOLIO_MEMORY_EMBED_MODEL", "") || null,
    memoryEmbeddingsUrl:
      str(getPath(file, "memory.embeddingsUrl"), "FOLIO_MEMORY_EMBED_URL", "") || null,

    defaultLocale: str(getPath(file, "locale"), "FOLIO_LOCALE", "pt-BR"),

    nodeWifiRetryMs: num(getPath(file, "node.wifiRetryMs"), "FOLIO_WIFI_RETRY_MS", 5000),
    nodePushBackoffMaxMs: num(getPath(file, "node.pushBackoffMaxMs"), "FOLIO_PUSH_BACKOFF_MAX_MS", 30000),
    nodeStatusIntervalMs: num(getPath(file, "node.statusIntervalMs"), "FOLIO_STATUS_INTERVAL_MS", 15000),
  };
}

export function applyCfgTo(target, file = getFileData()) {
  Object.assign(target, buildCfgFromFile(file));
}

initConfigStore();
