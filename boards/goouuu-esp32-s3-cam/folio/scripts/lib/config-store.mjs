import { createHash } from "node:crypto";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { homedir } from "node:os";
import { dirname, join } from "node:path";

const FRAME_SIZE_TO_ID = { CIF: 5, QVGA: 6, VGA: 7, SVGA: 8, XGA: 9 };
const ID_TO_FRAME_SIZE = Object.fromEntries(
  Object.entries(FRAME_SIZE_TO_ID).map(([k, v]) => [String(v), k]),
);

export const DEFAULT_CONFIG = {
  locale: "pt-BR",
  port: 8770,
  dataDir: null,
  lm: {
    url: "http://127.0.0.1:1234/v1/chat/completions",
    modelFast: "mistralai/ministral-3-3b",
    modelDeep: "mistralai/ministral-3-3b",
  },
  frames: {
    captureIntervalMs: 60000,
    captionIntervalMs: 60000,
    captionMaxTokens: 220,
    captionTemperature: 0.05,
    pipelineBatch: 1,
    jpegQuality: 12,
    size: "QVGA",
  },
  audio: {
    chunkMs: 1000,
    sampleRate: 16000,
    speechEnergyThreshold: 0.008,
    whisperBin: "whisper",
    whisperModel: "base",
    whisperDevice: "cpu",
    whisperTimeoutMs: 120000,
    whisperLanguage: null,
    pipelineBatch: 4,
    retentionDays: 7,
  },
  pipeline: { enabled: true, intervalMs: 30000 },
  digest: { auto: true, intervalMs: 1800000 },
  episodes: { gapMin: 12 },
  memory: {
    enabled: true,
    lookbackDays: 90,
    retrieveLimit: 14,
    minScore: 0.08,
    useEmbeddings: false,
    embeddingModel: null,
    embeddingsUrl: null,
  },
  node: {
    wifiRetryMs: 5000,
    pushBackoffMaxMs: 30000,
    statusIntervalMs: 15000,
  },
};

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

let configPath = null;
let fileData = clone(DEFAULT_CONFIG);

export function initConfigStore() {
  for (const path of configPaths()) {
    if (existsSync(path)) {
      configPath = path;
      fileData = deepMerge(clone(DEFAULT_CONFIG), JSON.parse(readFileSync(path, "utf8")));
      return { path, data: fileData };
    }
  }
  return { path: null, data: fileData };
}

export function getConfigPath() {
  return configPath;
}

export function getFileData() {
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
  const data = clone(fileData);
  return {
    configPath,
    version: nodeConfigVersion(),
    ...data,
    node: nodeConfigPayload().node,
  };
}

export function nodeConfigPayload() {
  const size = String(fileData.frames?.size ?? "QVGA").toUpperCase();
  return {
    version: nodeConfigVersion(),
    frames: {
      captureIntervalMs: Number(fileData.frames?.captureIntervalMs ?? 60000),
      jpegQuality: Number(fileData.frames?.jpegQuality ?? 12),
      size,
      sizeId: frameSizeId(size),
    },
    audio: {
      chunkMs: Number(fileData.audio?.chunkMs ?? 1000),
      sampleRate: Number(fileData.audio?.sampleRate ?? 16000),
    },
    node: {
      wifiRetryMs: Number(fileData.node?.wifiRetryMs ?? 5000),
      pushBackoffMaxMs: Number(fileData.node?.pushBackoffMaxMs ?? 30000),
      statusIntervalMs: Number(fileData.node?.statusIntervalMs ?? 15000),
    },
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

export { ID_TO_FRAME_SIZE, FRAME_SIZE_TO_ID };
