import { execSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const FRAME_SIZE_TO_ID = { CIF: 5, QVGA: 6, VGA: 7, SVGA: 8, XGA: 9 };

const EXAMPLE_PATH = join(
  dirname(fileURLToPath(import.meta.url)),
  "../../folio.config.example.json",
);

export const DEFAULT_CONFIG = JSON.parse(readFileSync(EXAMPLE_PATH, "utf8"));

function getPath(obj, dotPath) {
  return dotPath.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

function envNum(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return Number(process.env[envKey]);
  }
  return fileVal !== undefined ? Number(fileVal) : fallback;
}

function envStr(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return process.env[envKey];
  }
  return fileVal !== undefined ? String(fileVal) : fallback;
}

function envBool(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return process.env[envKey] !== "0" && process.env[envKey] !== "false";
  }
  return fileVal !== undefined ? Boolean(fileVal) : fallback;
}

function cfgNum(file, dotPath, envKey) {
  return envNum(getPath(file, dotPath), envKey, getPath(DEFAULT_CONFIG, dotPath));
}

function cfgStr(file, dotPath, envKey) {
  return envStr(getPath(file, dotPath), envKey, getPath(DEFAULT_CONFIG, dotPath));
}

function cfgBool(file, dotPath, envKey) {
  return envBool(getPath(file, dotPath), envKey, getPath(DEFAULT_CONFIG, dotPath));
}

function configPaths() {
  const paths = [];
  if (process.env.FOLIO_CONFIG) {
    paths.push(process.env.FOLIO_CONFIG);
  }
  paths.push(join(homedir(), ".folio", "config.json"));
  return paths;
}

function deepMerge(base, patch) {
  if (!patch || typeof patch !== "object") {
    return base;
  }
  const out = Array.isArray(base) ? [...base] : { ...base };
  for (const [k, v] of Object.entries(patch)) {
    if (v && typeof v === "object" && !Array.isArray(v) && typeof out[k] === "object") {
      out[k] = deepMerge(out[k], v);
    } else if (v !== undefined) {
      out[k] = v;
    }
  }
  return out;
}

function clone(obj) {
  return JSON.parse(JSON.stringify(obj));
}

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

let configPath = null;
let fileData = clone(DEFAULT_CONFIG);

function loadFileConfig() {
  for (const path of configPaths()) {
    if (existsSync(path)) {
      configPath = path;
      fileData = deepMerge(clone(DEFAULT_CONFIG), JSON.parse(readFileSync(path, "utf8")));
      return;
    }
  }
}

function getFileData() {
  return fileData;
}

export function frameSizeId(sizeName) {
  return FRAME_SIZE_TO_ID[String(sizeName ?? "QVGA").toUpperCase()] ?? 6;
}

export function nodeConfigVersion(data = fileData) {
  const payload = {
    frames: {
      captureIntervalMs: data.frames?.captureIntervalMs,
      jpegQuality: data.frames?.jpegQuality,
      size: data.frames?.size,
    },
    audio: {
      chunkMs: data.audio?.chunkMs,
      sampleRate: data.audio?.sampleRate,
    },
    node: data.node,
  };
  return createHash("sha256").update(JSON.stringify(payload)).digest("hex").slice(0, 12);
}

export function editableConfig() {
  return {
    configPath,
    version: nodeConfigVersion(),
    ...clone(fileData),
  };
}

export const publicConfig = editableConfig;

export function nodeConfigPayload() {
  const { frames, audio, node } = fileData;
  const size = String(frames.size).toUpperCase();
  return {
    version: nodeConfigVersion(),
    frames: {
      captureIntervalMs: frames.captureIntervalMs,
      jpegQuality: frames.jpegQuality,
      size,
      sizeId: frameSizeId(size),
    },
    audio: {
      chunkMs: audio.chunkMs,
      sampleRate: audio.sampleRate,
    },
    node: { ...node },
    compileTimeNote:
      "audio chunkMs/sampleRate and frame sizeId apply only if firmware was built with matching FOLIO_* defaults; interval/jpegQuality/wifi sync at runtime.",
  };
}

const RESTART_KEYS = new Set([
  "lm",
  "audio.whisperBin",
  "audio.whisperModel",
  "audio.whisperDevice",
  "port",
  "dataDir",
]);

function patchNeedsRestart(patch, prefix = "") {
  for (const [k, v] of Object.entries(patch ?? {})) {
    const path = prefix ? `${prefix}.${k}` : k;
    if (v && typeof v === "object" && !Array.isArray(v)) {
      if (patchNeedsRestart(v, path)) {
        return true;
      }
    } else if (RESTART_KEYS.has(path) || RESTART_KEYS.has(k)) {
      return true;
    }
  }
  return false;
}

export function saveConfigPatch(patch) {
  fileData = deepMerge(fileData, patch);
  const targetPath = configPath ?? configPaths()[configPaths().length - 1];
  mkdirSync(dirname(targetPath), { recursive: true });
  writeFileSync(targetPath, `${JSON.stringify(fileData, null, 2)}\n`, "utf8");
  configPath = targetPath;
  const version = nodeConfigVersion();
  return {
    ok: true,
    configPath: targetPath,
    version,
    restartRecommended: patchNeedsRestart(patch),
  };
}

function buildCfgFromFile(file = getFileData()) {
  const dataDir =
    envStr(getPath(file, "dataDir"), "FOLIO_DATA_DIR", "") || join(homedir(), ".folio");
  return {
    configPath,
    port: cfgNum(file, "port", "FOLIO_PORT"),
    dataDir,

    lmUrl: cfgStr(file, "lm.url", "LM_URL"),
    modelFast: cfgStr(file, "lm.modelFast", "FOLIO_MODEL_FAST"),
    modelDeep: cfgStr(file, "lm.modelDeep", "FOLIO_MODEL_DEEP"),

    frameCaptureIntervalMs: cfgNum(file, "frames.captureIntervalMs", "FOLIO_FRAME_INTERVAL_MS"),
    frameCaptionIntervalMs: cfgNum(file, "frames.captionIntervalMs", "FOLIO_FRAME_CAPTION_MS"),
    frameCaptionMaxTokens: cfgNum(file, "frames.captionMaxTokens", "FOLIO_FRAME_CAPTION_MAX_TOKENS"),
    frameCaptionTemperature: cfgNum(file, "frames.captionTemperature", "FOLIO_FRAME_CAPTION_TEMP"),
    pipelineFrameBatch: cfgNum(file, "frames.pipelineBatch", "FOLIO_PIPELINE_FRAME_BATCH"),
    frameJpegQuality: cfgNum(file, "frames.jpegQuality", "FOLIO_JPEG_QUALITY"),
    frameSize: cfgStr(file, "frames.size", "FOLIO_FRAME_SIZE"),

    audioChunkMs: cfgNum(file, "audio.chunkMs", "FOLIO_AUDIO_CHUNK_MS"),
    audioSampleRate: cfgNum(file, "audio.sampleRate", "FOLIO_AUDIO_SAMPLE_RATE"),
    speechEnergyThreshold: cfgNum(file, "audio.speechEnergyThreshold", "FOLIO_SPEECH_ENERGY"),
    whisperBin: resolveWhisperBin(
      cfgStr(file, "audio.whisperBin", "FOLIO_WHISPER_BIN"),
      process.env.FOLIO_WHISPER_BIN,
    ),
    whisperModel: cfgStr(file, "audio.whisperModel", "FOLIO_WHISPER_MODEL"),
    whisperDevice: cfgStr(file, "audio.whisperDevice", "FOLIO_WHISPER_DEVICE"),
    whisperTimeoutMs: cfgNum(file, "audio.whisperTimeoutMs", "FOLIO_WHISPER_TIMEOUT_MS"),
    whisperLanguage: cfgStr(file, "audio.whisperLanguage", "FOLIO_WHISPER_LANGUAGE") || null,
    pipelineAudioBatch: cfgNum(file, "audio.pipelineBatch", "FOLIO_PIPELINE_AUDIO_BATCH"),
    audioRetentionDays: cfgNum(file, "audio.retentionDays", "FOLIO_AUDIO_RETENTION_DAYS"),

    pipelineIntervalMs: cfgNum(file, "pipeline.intervalMs", "FOLIO_PIPELINE_INTERVAL_MS"),
    pipelineEnabled: cfgBool(file, "pipeline.enabled", "FOLIO_PIPELINE"),

    digestIntervalMs: cfgNum(file, "digest.intervalMs", "FOLIO_DIGEST_INTERVAL_MS"),
    digestAuto: cfgBool(file, "digest.auto", "FOLIO_DIGEST_AUTO"),

    episodeGapMin: cfgNum(file, "episodes.gapMin", "FOLIO_EPISODE_GAP_MIN"),

    memoryEnabled: cfgBool(file, "memory.enabled", "FOLIO_MEMORY"),
    memoryLookbackDays: cfgNum(file, "memory.lookbackDays", "FOLIO_MEMORY_LOOKBACK_DAYS"),
    memoryRetrieveLimit: cfgNum(file, "memory.retrieveLimit", "FOLIO_MEMORY_RETRIEVE"),
    memoryMinScore: cfgNum(file, "memory.minScore", "FOLIO_MEMORY_MIN_SCORE"),
    memoryUseEmbeddings: cfgBool(file, "memory.useEmbeddings", "FOLIO_MEMORY_EMBEDDINGS"),
    memoryEmbeddingModel: cfgStr(file, "memory.embeddingModel", "FOLIO_MEMORY_EMBED_MODEL") || null,
    memoryEmbeddingsUrl: cfgStr(file, "memory.embeddingsUrl", "FOLIO_MEMORY_EMBED_URL") || null,

    defaultLocale: cfgStr(file, "locale", "FOLIO_LOCALE"),

    nodeWifiRetryMs: cfgNum(file, "node.wifiRetryMs", "FOLIO_WIFI_RETRY_MS"),
    nodePushBackoffMaxMs: cfgNum(file, "node.pushBackoffMaxMs", "FOLIO_PUSH_BACKOFF_MAX_MS"),
    nodeStatusIntervalMs: cfgNum(file, "node.statusIntervalMs", "FOLIO_STATUS_INTERVAL_MS"),
  };
}

function applyCfgTo(target, file = getFileData()) {
  Object.assign(target, buildCfgFromFile(file));
}

loadFileConfig();

export const CFG = buildCfgFromFile();

export function reloadConfig() {
  loadFileConfig();
  applyCfgTo(CFG);
  return editableConfig();
}

export const PATHS = {
  db: () => join(CFG.dataDir, "folio.db"),
  audioDir: (day) => join(CFG.dataDir, "audio", day),
  frameDir: (day) => join(CFG.dataDir, "frames", day),
  speakerDir: () => join(CFG.dataDir, "speakers"),
  digestDir: () => join(CFG.dataDir, "digests"),
};

export function updateConfig(patch) {
  const result = saveConfigPatch(patch);
  reloadConfig();
  return { ...result, config: editableConfig() };
}
