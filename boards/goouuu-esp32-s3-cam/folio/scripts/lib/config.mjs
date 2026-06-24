import { execSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";
import { DEFAULT_CONFIG } from "./defaults.mjs";
import { normalizeOpenAiBase } from "./llm.mjs";
import { brainUrlForClient, listLanUrls } from "./network.mjs";

export { DEFAULT_CONFIG };

/** esp_camera framesize_t IDs — must match firmware FOLIO_FRAME_SIZE_ID / platformio.ini */
const FRAME_SIZE_TO_ID = { CIF: 5, QVGA: 6, VGA: 7, SVGA: 8, XGA: 9 };

const SUPPORTED_LOCALES = new Set([
  "pt-BR", "pt-PT", "en-US", "en-GB", "es-ES", "fr-FR", "de-DE",
]);

function detectSystemLocale() {
  const raw = process.env.FOLIO_LOCALE || process.env.LANG || "";
  const normalized = raw.replace(/\.UTF-8/i, "").replace(/\.utf8/i, "").replace("_", "-");
  if (SUPPORTED_LOCALES.has(normalized)) {
    return normalized;
  }
  const base = normalized.split("-")[0];
  if (base === "pt") {
    return "pt-BR";
  }
  if (base === "en") {
    return "en-US";
  }
  if (base === "es") {
    return "es-ES";
  }
  if (base === "fr") {
    return "fr-FR";
  }
  if (base === "de") {
    return "de-DE";
  }
  try {
    const sys = Intl.DateTimeFormat().resolvedOptions().locale.replace("_", "-");
    if (SUPPORTED_LOCALES.has(sys)) {
      return sys;
    }
  } catch { /* ignore */ }
  return DEFAULT_CONFIG.locale;
}

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
let userOverrides = {};
let fileData = clone(DEFAULT_CONFIG);

function syncFileData() {
  fileData = deepMerge(clone(DEFAULT_CONFIG), userOverrides);
}

/** Disk stores only the user's model choice — everything else is code + bootstrap. */
function extractUserOverrides(raw) {
  if (!raw || typeof raw !== "object" || Array.isArray(raw)) {
    return {};
  }
  const legacy = clone(raw);
  migrateOpenAiToLm(legacy);

  const model =
    legacy.lm?.model ||
    legacy.lm?.modelFast ||
    legacy.openai?.model ||
    null;

  if (!model) {
    return {};
  }
  return { lm: { model } };
}

function compactStoredOverrides(overrides = userOverrides) {
  if (overrides.lm?.model) {
    return { lm: { model: overrides.lm.model } };
  }
  return {};
}

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
  if (!file.lm || typeof file.lm !== "object") {
    file.lm = {};
    changed = true;
  }
  if (file.memory?.embeddingModel && !file.lm.modelEmbed) {
    file.lm.modelEmbed = file.memory.embeddingModel;
    delete file.memory.embeddingModel;
    changed = true;
  }
  if (file.memory?.rerank?.model && !file.lm.modelRerank) {
    file.lm.modelRerank = file.memory.rerank.model;
    delete file.memory.rerank.model;
    changed = true;
  }
  if (file.audio?.whisperModel && !file.lm?.modelWhisper) {
    file.lm.modelWhisper = file.audio.whisperModel;
    changed = true;
  }
  return changed;
}


function persistFileConfig(path, data) {
  writeFileSync(path, `${JSON.stringify(data, null, 2)}\n`, "utf8");
}

export function userConfigOverrides() {
  return clone(compactStoredOverrides());
}

export function editableConfig() {
  return {
    configPath,
    version: nodeConfigVersion(),
    runtime: {
      speechEnergyThreshold: CFG.speechEnergyThreshold,
      lmUrl: CFG.lmBaseUrl,
      models: runtimeModels(),
    },
    ...clone(userConfigOverrides()),
  };
}

export const publicConfig = editableConfig;

function loadFileConfig() {
  configPath = null;
  userOverrides = {};
  for (const path of configPaths()) {
    if (existsSync(path)) {
      configPath = path;
      const raw = JSON.parse(readFileSync(path, "utf8"));
      userOverrides = extractUserOverrides(raw);
      const compact = compactStoredOverrides();
      if (JSON.stringify(raw) !== JSON.stringify(compact)) {
        persistFileConfig(path, compact);
        console.log(`[config] só modelo em ${path}`);
      }
      syncFileData();
      return;
    }
  }
  syncFileData();
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
    embed: CFG.lmModelEmbed,
    rerank: CFG.lmModelRerank,
  };
}

export function nodeConfigPayload(clientIp = null) {
  const size = String(CFG.frameSize).toUpperCase();
  const brainUrl = brainUrlForClient(clientIp);
  const node = fileData.node ?? DEFAULT_CONFIG.node ?? {};
  return {
    version: nodeConfigVersion(),
    brainUrl,
    brainUrls: listLanUrls(),
    frames: {
      captureIntervalMs: CFG.frameCaptureIntervalMs,
      jpegQuality: CFG.frameJpegQuality,
      motionCaptureMinMs: Math.max(
        3000,
        Math.min(30000, Math.floor((CFG.frameCaptureIntervalMs ?? 60000) / 10)),
      ),
      size,
      sizeId: frameSizeId(size),
    },
    audio: {
      chunkMs: CFG.vadFrameMs ?? CFG.audioChunkMs,
      sampleRate: CFG.audioSampleRate,
      speechEnergyThreshold: CFG.speechEnergyThreshold,
      vad: {
        frameMs: CFG.vadFrameMs,
        debounceMs: CFG.vadDebounceMs,
        silenceMs: CFG.vadSilenceMs,
        prerollMs: CFG.vadPrerollMs,
      },
    },
    perception: {
      motionMin: Math.max(0.04, CFG.perceptionMotionMin ?? 0.04),
      soundMinEnergy: CFG.perceptionSoundMinEnergy,
    },
    node: { ...node, brainUrl: CFG.nodeBrainUrl ?? node.brainUrl ?? null },
    compileTimeNote:
      "frame sizeId and audio buffer size need matching FOLIO_* at flash time; other fields sync at runtime.",
  };
}

const RESTART_KEYS = new Set(["port", "dataDir"]);

function patchNeedsRestart(patch) {
  for (const key of RESTART_KEYS) {
    if (!(key in patch)) {
      continue;
    }
    const newVal = patch[key];
    const oldVal = userOverrides[key] ?? DEFAULT_CONFIG[key];
    if (JSON.stringify(newVal) !== JSON.stringify(oldVal)) {
      return true;
    }
  }
  return false;
}

export function saveConfigPatch(patch) {
  if (patch?.lm?.model != null) {
    userOverrides.lm = { model: patch.lm.model };
  }
  syncFileData();
  const targetPath = configPath ?? join(homedir(), ".folio", "config.json");
  mkdirSync(dirname(targetPath), { recursive: true });
  const compact = compactStoredOverrides();
  persistFileConfig(targetPath, compact);
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
  const embed =
    cfgStr(file, "lm.modelEmbed", "LM_MODEL_EMBED") ||
    cfgStr(file, "memory.embeddingModel", "FOLIO_MEMORY_EMBED_MODEL") ||
    null;
  const rerank =
    cfgStr(file, "lm.modelRerank", "LM_MODEL_RERANK") ||
    cfgStr(file, "memory.rerank.model", "FOLIO_MEMORY_RERANK_MODEL") ||
    null;
  return { modelFast: model, modelDeep: deep, modelEmbed: embed, modelRerank: rerank };
}

function buildCfgFromFile(file = getFileData()) {
  const { modelFast, modelDeep, modelEmbed, modelRerank } = resolveLmModels(file);
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
    lmModelEmbed: modelEmbed,
    lmModelRerank: modelRerank,

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
    audioSttMaxNoSpeechProb: cfgNum(file, "audio.sttMaxNoSpeechProb", "FOLIO_STT_MAX_NO_SPEECH"),
    audioSttRejectPatterns: (() => {
      const custom = cfgStrArray(
        file,
        "audio.sttRejectPatterns",
        "FOLIO_STT_REJECT_PATTERNS",
        { noFallback: true },
      );
      if (custom.length) {
        return custom;
      }
      return DEFAULT_CONFIG.audio?.sttRejectPatterns ?? [];
    })(),
    audioChunkMs:
      cfgNum(file, "audio.vad.frameMs", "FOLIO_VAD_FRAME_MS") ||
      cfgNum(file, "audio.chunkMs", "FOLIO_AUDIO_CHUNK_MS"),
    audioSampleRate: cfgNum(file, "audio.sampleRate", "FOLIO_AUDIO_SAMPLE_RATE"),
    speechEnergyThreshold: 0,
    sttTimeoutMs: cfgNum(file, "audio.sttTimeoutMs", "FOLIO_STT_TIMEOUT_MS") ||
      cfgNum(file, "audio.whisperTimeoutMs", "FOLIO_WHISPER_TIMEOUT_MS"),
    sttLanguage: cfgStr(file, "audio.sttLanguage", "FOLIO_STT_LANGUAGE") ||
      cfgStr(file, "audio.whisperLanguage", "FOLIO_WHISPER_LANGUAGE") ||
      null,
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
    presentHideStaticFrames: cfgBool(file, "present.hideStaticFrames", "FOLIO_PRESENT_HIDE_STATIC"),
    presentHidePendingInFeed: cfgBool(file, "present.hidePendingInFeed", "FOLIO_PRESENT_HIDE_PENDING"),

    entitiesSoundKindEntity: cfgObject(file, "entities.soundKindEntity"),
    perceptionMotionForceMs: cfgNum(file, "perception.motionForceMs", "FOLIO_MOTION_FORCE_MS"),
    perceptionAutoEnhance: cfgBool(file, "perception.autoEnhance", "FOLIO_AUTO_ENHANCE"),
    perceptionVision: cfgObject(file, "perception.vision"),
    perceptionStoreSounds: cfgBool(file, "perception.storeSounds", "FOLIO_STORE_SOUNDS"),
    perceptionSoundMinEnergy: 0,
    perceptionSoundEngine: cfgStr(file, "perception.soundEngine", "FOLIO_SOUND_ENGINE"),
    perceptionYamnetMinScore: cfgNum(file, "perception.yamnetMinScore", "FOLIO_YAMNET_MIN_SCORE"),
    perceptionYamnetModelPath: cfgStr(file, "perception.yamnetModelPath", "FOLIO_YAMNET_MODEL") || null,
    perceptionYamnetLabelsPath: cfgStr(file, "perception.yamnetLabelsPath", "FOLIO_YAMNET_LABELS") || null,
    perceptionYamnetModelUrl: cfgStr(file, "perception.yamnetModelUrl", "FOLIO_YAMNET_MODEL_URL"),
    perceptionYamnetLabelsUrl: cfgStr(file, "perception.yamnetLabelsUrl", "FOLIO_YAMNET_LABELS_URL"),
    perceptionSoundLabels: cfgObject(file, "perception.soundLabels"),
    speakerMaxEnrollmentSamples: cfgNum(file, "speaker.maxEnrollmentSamples", "FOLIO_SPEAKER_MAX_SAMPLES"),

    perceptionMotionMin: 0,
    workerBacklogHigh: cfgNum(file, "worker.backlogHigh", "FOLIO_WORKER_BACKLOG_HIGH"),
    workerBacklogMedium: cfgNum(file, "worker.backlogMedium", "FOLIO_WORKER_BACKLOG_MEDIUM"),
    workerBatchMaxHigh: cfgNum(file, "worker.batchMaxHigh", "FOLIO_WORKER_BATCH_MAX_HIGH"),
    workerFrameSkipBatch: cfgNum(file, "worker.frameSkipBatch", "FOLIO_WORKER_FRAME_SKIP_BATCH"),
    workerBatchMaxMedium: cfgNum(file, "worker.batchMaxMedium", "FOLIO_WORKER_BATCH_MAX_MEDIUM"),

    digestPassDTemperature: cfgNum(file, "digest.passDTemperature", "FOLIO_DIGEST_PASS_D_TEMP"),

    memoryEnabled: cfgBool(file, "memory.enabled", "FOLIO_MEMORY"),
    memoryLookbackDays: cfgNum(file, "memory.lookbackDays", "FOLIO_MEMORY_LOOKBACK_DAYS"),
    memoryRetrieveLimit: cfgNum(file, "memory.retrieveLimit", "FOLIO_MEMORY_RETRIEVE"),
    memoryMinScore: cfgNum(file, "memory.minScore", "FOLIO_MEMORY_MIN_SCORE"),
    memoryUseEmbeddings: (() => {
      const v = getPath(file, "memory.useEmbeddings");
      if (v === false) {
        return false;
      }
      if (v === true) {
        return true;
      }
      return null;
    })(),
    memoryEmbedBatchSize: cfgNum(file, "memory.embedBatchSize", "FOLIO_MEMORY_EMBED_BATCH"),
    memoryMinUtteranceChars: cfgNum(file, "memory.minUtteranceChars", "FOLIO_MEMORY_MIN_UTT_CHARS"),
    memoryUtteranceGroupMs: cfgNum(file, "memory.utteranceGroupMs", "FOLIO_MEMORY_UTT_GROUP_MS"),
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
    memoryRerankCandidateLimit: cfgNum(file, "memory.rerank.candidateLimit", "FOLIO_MEMORY_RERANK_CANDIDATES"),
    memoryRerankTopK: cfgNum(file, "memory.rerank.topK", "FOLIO_MEMORY_RERANK_TOPK"),

    defaultLocale: cfgStr(file, "locale", "FOLIO_LOCALE") || detectSystemLocale(),

    nodeBrainUrl: cfgStr(file, "node.brainUrl", "FOLIO_BRAIN_URL") || null,

    whisperBin: resolveWhisperBin(getPath(file, "audio.whisperBin")),
    whisperModel: cfgStr(file, "audio.whisperModel", "FOLIO_WHISPER_MODEL") || null,
    whisperDevice: resolveWhisperDevice(
      getPath(file, "audio.whisperDevice"),
      process.env.FOLIO_WHISPER_DEVICE,
    ),
    lmModelWhisper: cfgStr(file, "lm.modelWhisper", "FOLIO_LM_MODEL_WHISPER") || null,

    audioSttEnabled: (() => {
      const env = process.env.FOLIO_STT_ENABLED;
      if (env !== undefined && env !== "") {
        return env !== "0" && env !== "false";
      }
      const v = getPath(file, "audio.sttEnabled");
      if (v === false) {
        return false;
      }
      if (v === true) {
        return true;
      }
      return null;
    })(),
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
