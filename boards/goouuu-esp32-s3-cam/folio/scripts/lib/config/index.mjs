import { execSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { normalizeOpenAiBase } from "../llm/openai-base.mjs";

/** esp_camera framesize_t IDs — must match firmware FOLIO_FRAME_SIZE_ID / platformio.ini */
const FRAME_SIZE_TO_ID = { CIF: 5, QVGA: 6, VGA: 7, SVGA: 8, XGA: 9 };

/** Auto-injected PT list — removed; stopWords must be explicit in user config. */
const LEGACY_MEMORY_STOP_WORDS = [
  "a", "o", "e", "de", "da", "do", "das", "dos", "em", "no", "na", "nos", "nas",
  "um", "uma", "para", "por", "com", "sem", "que", "se", "as", "os",
];

function isLegacyMemoryStopWords(list) {
  if (!Array.isArray(list) || list.length !== LEGACY_MEMORY_STOP_WORDS.length) {
    return false;
  }
  return list.every((w, i) => w === LEGACY_MEMORY_STOP_WORDS[i]);
}

const EXAMPLE_PATH = join(
  dirname(fileURLToPath(import.meta.url)),
  "../../../folio.config.example.json",
);

export const DEFAULT_CONFIG = JSON.parse(readFileSync(EXAMPLE_PATH, "utf8"));

function getPath(obj, dotPath) {
  return dotPath.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);
}

function envNum(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return Number(process.env[envKey]);
  }
  if (fileVal === null || fileVal === undefined) {
    return fallback;
  }
  return Number(fileVal);
}

function envStr(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return process.env[envKey];
  }
  if (fileVal === null || fileVal === undefined) {
    return fallback;
  }
  return String(fileVal);
}

function envBool(fileVal, envKey, fallback) {
  if (process.env[envKey] !== undefined && process.env[envKey] !== "") {
    return process.env[envKey] !== "0" && process.env[envKey] !== "false";
  }
  if (fileVal === null || fileVal === undefined) {
    return fallback;
  }
  return Boolean(fileVal);
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

function cfgStrArray(file, dotPath, envKey, { noFallback = false } = {}) {
  const env = process.env[envKey];
  if (env !== undefined && env !== "") {
    return env
      .split(",")
      .map((s) => s.trim())
      .filter(Boolean);
  }
  const val = getPath(file, dotPath);
  if (Array.isArray(val)) {
    return val.map(String);
  }
  if (noFallback) {
    return [];
  }
  const fallback = getPath(DEFAULT_CONFIG, dotPath);
  return Array.isArray(fallback) ? fallback.map(String) : [];
}

function cfgObject(file, dotPath) {
  const val = getPath(file, dotPath);
  if (val && typeof val === "object" && !Array.isArray(val)) {
    return val;
  }
  const fallback = getPath(DEFAULT_CONFIG, dotPath);
  return fallback && typeof fallback === "object" && !Array.isArray(fallback)
    ? clone(fallback)
    : {};
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

function cudaAvailable() {
  try {
    const out = execSync("nvidia-smi -L", {
      encoding: "utf8",
      stdio: ["ignore", "pipe", "ignore"],
      timeout: 3000,
    });
    return /GPU/i.test(out);
  } catch {
    return false;
  }
}

export function isCudaAvailable() {
  return cudaAvailable();
}

/** openai-whisper --device: cpu | cuda | mps. "auto" picks cuda when NVIDIA is present. */
function resolveWhisperDevice(fileVal, envVal) {
  let device = envVal || fileVal || "auto";
  if (device === "auto") {
    device = cudaAvailable() ? "cuda" : "cpu";
  }
  if (device === "cuda" && !cudaAvailable()) {
    console.warn("[whisper] cuda requested but no NVIDIA GPU — using cpu");
    return "cpu";
  }
  return device;
}

let configPath = null;
let fileData = clone(DEFAULT_CONFIG);

function migrateOpenAiToLm(file) {
  let changed = false;
  if (file.openai && typeof file.openai === "object") {
    if (!file.lm || typeof file.lm !== "object") {
      file.lm = {};
    }
    const o = file.openai;
    if (o.baseUrl && file.lm.url == null) {
      file.lm.url = normalizeOpenAiBase(o.baseUrl);
    }
    if (o.model && file.lm.model == null) {
      file.lm.model = o.model;
    }
    if (o.modelDeep != null && file.lm.modelDeep == null) {
      file.lm.modelDeep = o.modelDeep;
    }
    delete file.openai;
    changed = true;
  }
  // Legacy flat lm.modelFast
  if (file.lm?.modelFast && !file.lm.model) {
    file.lm.model = file.lm.modelFast;
    delete file.lm.modelFast;
    changed = true;
  }
  return changed;
}

/** Drop legacy keys; ensure lm, insights, audio.vad. */
function migrateConfigSchema(file) {
  let changed = false;

  if (file.digest && typeof file.digest === "object") {
    if (!file.insights || typeof file.insights !== "object") {
      file.insights = {};
    }
    const d = file.digest;
    if (d.auto != null && file.insights.auto == null) {
      file.insights.auto = d.auto;
    }
    if (d.passDTemperature != null && file.insights.temperature == null) {
      file.insights.temperature = d.passDTemperature;
    }
    delete file.digest;
    changed = true;
  }

  if (file.episodes) {
    delete file.episodes;
    changed = true;
  }

  if (!file.insights || typeof file.insights !== "object") {
    file.insights = { ...DEFAULT_CONFIG.insights };
    changed = true;
  } else {
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.insights)) {
      if (file.insights[k] == null && v != null) {
        file.insights[k] = v;
        changed = true;
      }
    }
  }

  if (!file.lm || typeof file.lm !== "object") {
    file.lm = { ...DEFAULT_CONFIG.lm };
    changed = true;
  }

  if (!file.perception || typeof file.perception !== "object") {
    file.perception = { ...DEFAULT_CONFIG.perception };
    changed = true;
  } else {
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.perception)) {
      if (file.perception[k] == null && v != null) {
        file.perception[k] = v;
        changed = true;
      }
    }
  }

  if (!file.audio || typeof file.audio !== "object") {
    file.audio = { ...DEFAULT_CONFIG.audio };
    changed = true;
  } else {
    if (file.audio.chunkMs != null) {
      delete file.audio.chunkMs;
      changed = true;
    }
    if (file.audio.ambientEnergyThreshold != null) {
      delete file.audio.ambientEnergyThreshold;
      changed = true;
    }
    if (file.audio.chunkMs != null && !file.audio.vad?.frameMs) {
      if (!file.audio.vad) {
        file.audio.vad = { ...DEFAULT_CONFIG.audio.vad };
      }
      file.audio.vad.frameMs = file.audio.chunkMs;
      changed = true;
    }
    if (!file.audio.vad || typeof file.audio.vad !== "object") {
      file.audio.vad = { ...DEFAULT_CONFIG.audio.vad };
      changed = true;
    } else {
      for (const [k, v] of Object.entries(DEFAULT_CONFIG.audio.vad)) {
        if (file.audio.vad[k] == null && v != null) {
          file.audio.vad[k] = v;
          changed = true;
        }
      }
    }
  }

  if (file.memory?.embeddingsUrl != null) {
    delete file.memory.embeddingsUrl;
    changed = true;
  }
  if (file.memory?.rerank?.url != null) {
    delete file.memory.rerank.url;
    changed = true;
  }
  if (file.memory?.fallbackLexical != null) {
    delete file.memory.fallbackLexical;
    changed = true;
  }

  if (!file.memory || typeof file.memory !== "object") {
    file.memory = { ...DEFAULT_CONFIG.memory };
    changed = true;
  } else {
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.memory)) {
      if (k === "lexical" || k === "rerank") {
        continue;
      }
      if (file.memory[k] == null && v != null) {
        file.memory[k] = v;
        changed = true;
      }
    }
    if (!file.memory.lexical || typeof file.memory.lexical !== "object") {
      file.memory.lexical = {
        minTokenLength: DEFAULT_CONFIG.memory.lexical?.minTokenLength ?? 3,
      };
      changed = true;
    } else {
      for (const [k, v] of Object.entries(DEFAULT_CONFIG.memory.lexical)) {
        if (k === "stopWords") {
          continue;
        }
        if (file.memory.lexical[k] == null && v != null) {
          file.memory.lexical[k] = v;
          changed = true;
        }
      }
    }
    if (isLegacyMemoryStopWords(file.memory.lexical?.stopWords)) {
      file.memory.lexical.stopWords = [];
      changed = true;
    }
  }

  if (!file.speaker || typeof file.speaker !== "object") {
    file.speaker = { ...DEFAULT_CONFIG.speaker };
    changed = true;
  } else {
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.speaker)) {
      if (file.speaker[k] == null && v != null) {
        file.speaker[k] = v;
        changed = true;
      }
    }
  }

  if (file.perception && typeof file.perception === "object") {
    for (const key of ["soundLabels", "yamnetKindMap", "heuristic"]) {
      if (!file.perception[key] || typeof file.perception[key] !== "object") {
        file.perception[key] = { ...DEFAULT_CONFIG.perception[key] };
        changed = true;
      } else {
        for (const [k, v] of Object.entries(DEFAULT_CONFIG.perception[key] ?? {})) {
          if (file.perception[key][k] == null && v != null) {
            file.perception[key][k] = v;
            changed = true;
          }
        }
      }
    }
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.perception)) {
      if (typeof v === "object") {
        continue;
      }
      if (file.perception[k] == null && v != null) {
        file.perception[k] = v;
        changed = true;
      }
    }
  }

  if (!file.present || typeof file.present !== "object") {
    file.present = { ...DEFAULT_CONFIG.present };
    changed = true;
  } else {
    if (!file.present.labels || typeof file.present.labels !== "object") {
      file.present.labels = { ...DEFAULT_CONFIG.present.labels };
      changed = true;
    }
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.present)) {
      if (k === "labels") {
        continue;
      }
      if (file.present[k] == null && v != null) {
        file.present[k] = v;
        changed = true;
      }
    }
  }

  if (!file.http || typeof file.http !== "object") {
    file.http = { ...DEFAULT_CONFIG.http };
    changed = true;
  } else {
    for (const [k, v] of Object.entries(DEFAULT_CONFIG.http)) {
      if (file.http[k] == null && v != null) {
        file.http[k] = v;
        changed = true;
      }
    }
  }

  if (!file.entities || typeof file.entities !== "object") {
    file.entities = { ...DEFAULT_CONFIG.entities };
    changed = true;
  } else if (!file.entities.soundKindEntity) {
    file.entities.soundKindEntity = { ...DEFAULT_CONFIG.entities.soundKindEntity };
    changed = true;
  }

  if (file.frames && !file.frames.backlogGapMs) {
    file.frames.backlogGapMs = { ...DEFAULT_CONFIG.frames.backlogGapMs };
    changed = true;
  }
  if (file.frames?.staticSummary == null && DEFAULT_CONFIG.frames.staticSummary) {
    file.frames.staticSummary = DEFAULT_CONFIG.frames.staticSummary;
    changed = true;
  }

  return changed;
}

function persistFileConfig(path, data) {
  writeFileSync(path, `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

function runConfigMigrations(file, path) {
  const lm = migrateOpenAiToLm(file);
  const schema = migrateConfigSchema(file);
  if (lm || schema) {
    persistFileConfig(path, file);
    if (lm) {
      console.log(`[config] migrated openai.* → lm.* (local only) in ${path}`);
    }
    if (schema) {
      console.log(`[config] migrated schema (lm, vad, insights) in ${path}`);
    }
  }
}

function loadFileConfig() {
  for (const path of configPaths()) {
    if (existsSync(path)) {
      configPath = path;
      fileData = deepMerge(clone(DEFAULT_CONFIG), JSON.parse(readFileSync(path, "utf8")));
      runConfigMigrations(fileData, path);
      return;
    }
  }
}

function getFileData() {
  return fileData;
}

export function frameSizeId(sizeName) {
  const key = String(sizeName ?? DEFAULT_CONFIG.frames.size).toUpperCase();
  const id = FRAME_SIZE_TO_ID[key];
  if (id === undefined) {
    throw new Error(`Unknown frame size: ${key}`);
  }
  return id;
}

export function nodeConfigVersion(data = fileData) {
  const payload = {
    frames: {
      captureIntervalMs: data.frames?.captureIntervalMs,
      jpegQuality: data.frames?.jpegQuality,
      size: data.frames?.size,
    },
    audio: {
      chunkMs: data.audio?.vad?.frameMs ?? data.audio?.chunkMs,
      sampleRate: data.audio?.sampleRate,
      speechEnergyThreshold: data.audio?.speechEnergyThreshold,
      vad: data.audio?.vad,
    },
    perception: {
      motionMin: data.perception?.motionMin,
      soundMinEnergy: data.perception?.soundMinEnergy,
    },
    node: data.node,
  };
  return createHash("sha256").update(JSON.stringify(payload)).digest("hex").slice(0, 12);
}

function runtimeModels() {
  return {
    fast: CFG.modelFast,
    deep: CFG.modelDeep,
    whisper: CFG.whisperModel,
    embed: CFG.memoryEmbeddingModel || CFG.modelFast,
    rerank: CFG.memoryRerankModel,
    whisperDevice: CFG.whisperDevice,
  };
}

export function editableConfig() {
  return {
    configPath,
    version: nodeConfigVersion(),
    runtime: {
      whisperDeviceEffective: CFG.whisperDevice,
      cudaAvailable: cudaAvailable(),
      speechEnergyThreshold: CFG.speechEnergyThreshold,
      lmUrl: CFG.lmBaseUrl,
      models: runtimeModels(),
    },
    ...clone(fileData),
  };
}

export const publicConfig = editableConfig;

export function nodeConfigPayload() {
  const { frames, audio, node, perception } = fileData;
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
      chunkMs: audio.vad?.frameMs ?? audio.chunkMs ?? DEFAULT_CONFIG.audio.vad.frameMs,
      sampleRate: audio.sampleRate,
      speechEnergyThreshold: audio.speechEnergyThreshold,
      vad: { ...audio.vad },
    },
    perception: {
      motionMin: perception?.motionMin,
      soundMinEnergy: perception?.soundMinEnergy,
    },
    node: { ...node },
    compileTimeNote:
      "frame sizeId and audio buffer size need matching FOLIO_* at flash time; other fields sync at runtime.",
  };
}

const RESTART_KEYS = new Set(["port", "dataDir"]);

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
  runConfigMigrations(fileData, configPath ?? configPaths()[configPaths().length - 1]);
  const targetPath = configPath ?? configPaths()[configPaths().length - 1];
  mkdirSync(dirname(targetPath), { recursive: true });
  persistFileConfig(targetPath, fileData);
  configPath = targetPath;
  const version = nodeConfigVersion();
  return {
    ok: true,
    configPath: targetPath,
    version,
    restartRecommended: patchNeedsRestart(patch),
  };
}

function resolveLmBase(file) {
  const url =
    envStr(getPath(file, "lm.url"), "LM_URL", null) ||
    envStr(getPath(file, "openai.baseUrl"), "OPENAI_BASE_URL", null) ||
    envStr(getPath(file, "lm.url"), "FOLIO_LM_URL", null);
  if (url) {
    return normalizeOpenAiBase(url);
  }
  return normalizeOpenAiBase(getPath(DEFAULT_CONFIG, "lm.url"));
}

function resolveLmModels(file) {
  const model =
    cfgStr(file, "lm.model", "LM_MODEL") ||
    cfgStr(file, "openai.model", "OPENAI_MODEL") ||
    cfgStr(file, "lm.modelFast", "FOLIO_MODEL_FAST");
  const deep =
    cfgStr(file, "lm.modelDeep", "LM_MODEL_DEEP") ||
    cfgStr(file, "openai.modelDeep", "OPENAI_MODEL_DEEP") ||
    model;
  return { modelFast: model, modelDeep: deep };
}

function buildCfgFromFile(file = getFileData()) {
  const { modelFast, modelDeep } = resolveLmModels(file);
  const dataDir =
    envStr(getPath(file, "dataDir"), "FOLIO_DATA_DIR", "") || join(homedir(), ".folio");
  return {
    configPath,
    port: cfgNum(file, "port", "FOLIO_PORT"),
    dataDir,

    lmBaseUrl: resolveLmBase(file),
    openaiBaseUrl: resolveLmBase(file),
    openaiApiKey: null,
    modelFast,
    modelDeep,

    vadFrameMs: cfgNum(file, "audio.vad.frameMs", "FOLIO_VAD_FRAME_MS"),
    vadDebounceMs: cfgNum(file, "audio.vad.debounceMs", "FOLIO_VAD_DEBOUNCE_MS"),
    vadSilenceMs: cfgNum(file, "audio.vad.silenceMs", "FOLIO_VAD_SILENCE_MS"),
    vadPrerollMs: cfgNum(file, "audio.vad.prerollMs", "FOLIO_VAD_PREROLL_MS"),

    frameCaptureIntervalMs: cfgNum(file, "frames.captureIntervalMs", "FOLIO_FRAME_INTERVAL_MS"),
    frameCaptionIntervalMs: cfgNum(file, "frames.captionIntervalMs", "FOLIO_FRAME_CAPTION_MS"),
    frameCaptionMaxTokens: cfgNum(file, "frames.captionMaxTokens", "FOLIO_FRAME_CAPTION_MAX_TOKENS"),
    frameCaptionTemperature: cfgNum(file, "frames.captionTemperature", "FOLIO_FRAME_CAPTION_TEMP"),
    pipelineFrameBatch: cfgNum(file, "frames.pipelineBatch", "FOLIO_PIPELINE_FRAME_BATCH"),
    frameJpegQuality: cfgNum(file, "frames.jpegQuality", "FOLIO_JPEG_QUALITY"),
    frameSize: cfgStr(file, "frames.size", "FOLIO_FRAME_SIZE"),
    frameStaticSummary: cfgStr(file, "frames.staticSummary", "FOLIO_FRAME_STATIC_SUMMARY"),
    frameBacklogGap: cfgObject(file, "frames.backlogGapMs"),

    lmChatMaxTokens: cfgNum(file, "lm.chatMaxTokens", "FOLIO_LM_CHAT_MAX_TOKENS"),
    lmChatMaxTokensDeep: cfgNum(file, "lm.chatMaxTokensDeep", "FOLIO_LM_CHAT_MAX_TOKENS_DEEP"),

    audioSttMaxAttempts: cfgNum(file, "audio.sttMaxAttempts", "FOLIO_STT_MAX_ATTEMPTS"),
    audioChunkMs:
      cfgNum(file, "audio.vad.frameMs", "FOLIO_VAD_FRAME_MS") ||
      cfgNum(file, "audio.chunkMs", "FOLIO_AUDIO_CHUNK_MS"),
    audioSampleRate: cfgNum(file, "audio.sampleRate", "FOLIO_AUDIO_SAMPLE_RATE"),
    speechEnergyThreshold: cfgNum(file, "audio.speechEnergyThreshold", "FOLIO_SPEECH_ENERGY"),
    whisperBin: resolveWhisperBin(
      cfgStr(file, "audio.whisperBin", "FOLIO_WHISPER_BIN"),
      process.env.FOLIO_WHISPER_BIN,
    ),
    whisperModel: cfgStr(file, "audio.whisperModel", "FOLIO_WHISPER_MODEL"),
    whisperDevice: resolveWhisperDevice(
      getPath(file, "audio.whisperDevice"),
      process.env.FOLIO_WHISPER_DEVICE,
    ),
    whisperTimeoutMs: cfgNum(file, "audio.whisperTimeoutMs", "FOLIO_WHISPER_TIMEOUT_MS"),
    whisperLanguage: cfgStr(file, "audio.whisperLanguage", "FOLIO_WHISPER_LANGUAGE") || null,
    pipelineAudioBatch: cfgNum(file, "audio.pipelineBatch", "FOLIO_PIPELINE_AUDIO_BATCH"),
    audioRetentionDays: cfgNum(file, "audio.retentionDays", "FOLIO_AUDIO_RETENTION_DAYS"),
    audioRetentionSweepMs: cfgNum(file, "audio.retentionSweepMs", "FOLIO_AUDIO_RETENTION_SWEEP_MS"),

    pipelineIntervalMs: cfgNum(file, "pipeline.intervalMs", "FOLIO_PIPELINE_INTERVAL_MS"),
    pipelineEnabled: cfgBool(file, "pipeline.enabled", "FOLIO_PIPELINE"),

    digestIntervalMs: cfgNum(file, "insights.intervalMs", "FOLIO_INSIGHTS_INTERVAL_MS") ||
      cfgNum(file, "digest.intervalMs", "FOLIO_DIGEST_INTERVAL_MS"),
    digestAuto: cfgBool(file, "insights.auto", "FOLIO_INSIGHTS_AUTO") ||
      cfgBool(file, "digest.auto", "FOLIO_DIGEST_AUTO"),
    insightsIntervalMs: cfgNum(file, "insights.intervalMs", "FOLIO_INSIGHTS_INTERVAL_MS") ||
      cfgNum(file, "digest.intervalMs", "FOLIO_DIGEST_INTERVAL_MS"),
    insightsAuto: cfgBool(file, "insights.auto", "FOLIO_INSIGHTS_AUTO") ||
      cfgBool(file, "digest.auto", "FOLIO_DIGEST_AUTO"),
    insightsTemperature: cfgNum(file, "insights.temperature", "FOLIO_INSIGHTS_TEMP") ||
      cfgNum(file, "digest.passDTemperature", "FOLIO_DIGEST_PASS_D_TEMP"),
    insightsMaxTokens: cfgNum(file, "insights.maxTokens", "FOLIO_INSIGHTS_MAX_TOKENS"),
    insightsSampleUtterances: cfgNum(file, "insights.sampleUtterances", "FOLIO_INSIGHTS_SAMPLE_UTT"),
    insightsSampleFrames: cfgNum(file, "insights.sampleFrames", "FOLIO_INSIGHTS_SAMPLE_FRAMES"),

    httpBodyMaxBytes: cfgNum(file, "http.bodyMaxBytes", "FOLIO_HTTP_BODY_MAX"),
    httpIngestAudioMaxBytes: cfgNum(file, "http.ingestAudioMaxBytes", "FOLIO_HTTP_INGEST_AUDIO_MAX"),
    httpIngestFrameMaxBytes: cfgNum(file, "http.ingestFrameMaxBytes", "FOLIO_HTTP_INGEST_FRAME_MAX"),
    httpIngestEventMaxBytes: cfgNum(file, "http.ingestEventMaxBytes", "FOLIO_HTTP_INGEST_EVENT_MAX"),
    httpConfigPatchMaxBytes: cfgNum(file, "http.configPatchMaxBytes", "FOLIO_HTTP_CONFIG_PATCH_MAX"),

    presentSpeechGapMs: cfgNum(file, "present.speechGapMs", "FOLIO_PRESENT_SPEECH_GAP_MS"),
    presentSceneGapMs: cfgNum(file, "present.sceneGapMs", "FOLIO_PRESENT_SCENE_GAP_MS"),
    presentSoundGapMs: cfgNum(file, "present.soundGapMs", "FOLIO_PRESENT_SOUND_GAP_MS"),
    presentLabels: cfgObject(file, "present.labels"),

    entitiesSoundKindEntity: cfgObject(file, "entities.soundKindEntity"),
    perceptionMotionForceMs: cfgNum(file, "perception.motionForceMs", "FOLIO_MOTION_FORCE_MS"),
    perceptionAutoEnhance: cfgBool(file, "perception.autoEnhance", "FOLIO_AUTO_ENHANCE"),
    perceptionStoreSounds: cfgBool(file, "perception.storeSounds", "FOLIO_STORE_SOUNDS"),
    perceptionSoundMinEnergy: cfgNum(file, "perception.soundMinEnergy", "FOLIO_SOUND_MIN_ENERGY"),
    perceptionSoundMinConfidence: cfgNum(
      file,
      "perception.soundMinConfidence",
      "FOLIO_SOUND_MIN_CONF",
    ),
    perceptionSoundEngine: cfgStr(file, "perception.soundEngine", "FOLIO_SOUND_ENGINE"),
    perceptionYamnetMinScore: cfgNum(file, "perception.yamnetMinScore", "FOLIO_YAMNET_MIN_SCORE"),
    perceptionYamnetModelPath: cfgStr(file, "perception.yamnetModelPath", "FOLIO_YAMNET_MODEL") || null,
    perceptionYamnetLabelsPath: cfgStr(file, "perception.yamnetLabelsPath", "FOLIO_YAMNET_LABELS") || null,
    perceptionYamnetModelUrl: cfgStr(file, "perception.yamnetModelUrl", "FOLIO_YAMNET_MODEL_URL"),
    perceptionYamnetLabelsUrl: cfgStr(file, "perception.yamnetLabelsUrl", "FOLIO_YAMNET_LABELS_URL"),
    perceptionSoundLabels: cfgObject(file, "perception.soundLabels"),
    perceptionYamnetKindMap: cfgObject(file, "perception.yamnetKindMap"),
    perceptionHeuristic: cfgObject(file, "perception.heuristic"),

    speakerMinMatchScore: cfgNum(file, "speaker.minMatchScore", "FOLIO_SPEAKER_MIN_MATCH"),
    speakerMaxEnrollmentSamples: cfgNum(file, "speaker.maxEnrollmentSamples", "FOLIO_SPEAKER_MAX_SAMPLES"),

    perceptionMotionMin: cfgNum(file, "perception.motionMin", "FOLIO_MOTION_MIN"),
    workerBacklogMedium: cfgNum(file, "worker.backlogMedium", "FOLIO_WORKER_BACKLOG_MEDIUM"),
    workerBatchMaxHigh: cfgNum(file, "worker.batchMaxHigh", "FOLIO_WORKER_BATCH_MAX_HIGH"),
    workerBatchMaxMedium: cfgNum(file, "worker.batchMaxMedium", "FOLIO_WORKER_BATCH_MAX_MEDIUM"),

    digestPassDTemperature: cfgNum(file, "digest.passDTemperature", "FOLIO_DIGEST_PASS_D_TEMP"),

    memoryEnabled: cfgBool(file, "memory.enabled", "FOLIO_MEMORY"),
    memoryLookbackDays: cfgNum(file, "memory.lookbackDays", "FOLIO_MEMORY_LOOKBACK_DAYS"),
    memoryRetrieveLimit: cfgNum(file, "memory.retrieveLimit", "FOLIO_MEMORY_RETRIEVE"),
    memoryMinScore: cfgNum(file, "memory.minScore", "FOLIO_MEMORY_MIN_SCORE"),
    memoryUseEmbeddings: cfgBool(file, "memory.useEmbeddings", "FOLIO_MEMORY_EMBEDDINGS"),
    memoryEmbeddingModel: cfgStr(file, "memory.embeddingModel", "FOLIO_MEMORY_EMBED_MODEL") || null,
    memoryEmbeddingsUrl: null,
    memoryContextQueryTemplate: cfgStr(
      file,
      "memory.contextQueryTemplate",
      "FOLIO_MEMORY_CONTEXT_QUERY",
    ),
    memoryLexicalMinTokenLength: cfgNum(
      file,
      "memory.lexical.minTokenLength",
      "FOLIO_MEMORY_MIN_TOKEN_LEN",
    ),
    memoryLexicalStopWords: cfgStrArray(
      file,
      "memory.lexical.stopWords",
      "FOLIO_MEMORY_STOP_WORDS",
      { noFallback: true },
    ),
    memoryGraphRetrieveLimit: cfgNum(file, "memory.graphRetrieveLimit", "FOLIO_MEMORY_GRAPH_LIMIT"),
    memoryGraphMinScore: cfgNum(file, "memory.graphMinScore", "FOLIO_MEMORY_GRAPH_MIN_SCORE"),
    memoryProfileLimit: cfgNum(file, "memory.profileLimit", "FOLIO_MEMORY_PROFILE_LIMIT"),
    memoryMinFactTextLength: cfgNum(file, "memory.minFactTextLength", "FOLIO_MEMORY_MIN_FACT_LEN"),

    memoryRerankEnabled: cfgBool(file, "memory.rerank.enabled", "FOLIO_MEMORY_RERANK"),
    memoryRerankModel: cfgStr(file, "memory.rerank.model", "FOLIO_MEMORY_RERANK_MODEL") || null,
    memoryRerankUrl: null,
    memoryRerankCandidateLimit: cfgNum(file, "memory.rerank.candidateLimit", "FOLIO_MEMORY_RERANK_CANDIDATES"),
    memoryRerankTopK: cfgNum(file, "memory.rerank.topK", "FOLIO_MEMORY_RERANK_TOPK"),

    defaultLocale: cfgStr(file, "locale", "FOLIO_LOCALE"),
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
  modelsDir: () => join(CFG.dataDir, "models"),
};

export function updateConfig(patch) {
  const result = saveConfigPatch(patch);
  reloadConfig();
  const config = editableConfig();
  return { ...result, config };
}
