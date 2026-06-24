/**
 * Runtime bootstrap — auto-detect models, STT, embeddings; tune worker without user config.
 */
import { CFG, reloadConfig } from "./config.mjs";
import { fetchOpenAiModels } from "./llm.mjs";
import { refreshSttCapability } from "./stt-capability.mjs";

const runtime = {
  bootstrapped: false,
  models: null,
  stt: null,
  embeddings: false,
  notes: [],
};

function pickChatModel(catalog, configured) {
  const chat = catalog.chat ?? [];
  if (configured && chat.includes(configured)) {
    return configured;
  }
  if (configured && catalog.all?.includes(configured)) {
    return configured;
  }
  const vision = chat.find((id) => /vision|vl-|llava|ministral|pixtral|gemma.*vision/i.test(id));
  return vision ?? chat[0] ?? configured ?? null;
}

function pickEmbedModel(catalog, configured) {
  const embed = catalog.embed ?? [];
  if (configured && embed.includes(configured)) {
    return configured;
  }
  return embed[0] ?? configured ?? null;
}

function pickDeepModel(fast, configured) {
  if (configured && configured !== fast) {
    return configured;
  }
  return fast;
}

/** Apply detected capabilities onto live CFG (in-memory; file unchanged). */
export async function bootstrapRuntime({ force = false } = {}) {
  if (runtime.bootstrapped && !force) {
    return runtimeSummary();
  }

  runtime.notes = [];
  const [stt, catalog] = await Promise.all([
    refreshSttCapability({ force }),
    fetchOpenAiModels(),
  ]);

  runtime.stt = stt;
  runtime.models = catalog;

  if (catalog.ok) {
    const fast = pickChatModel(catalog, CFG.modelFast);
    const deep = pickDeepModel(fast, CFG.modelDeep);
    const embed = pickEmbedModel(catalog, CFG.lmModelEmbed);

    if (fast && fast !== CFG.modelFast) {
      runtime.notes.push(`model → ${fast}`);
      CFG.modelFast = fast;
    }
    if (deep && deep !== CFG.modelDeep) {
      CFG.modelDeep = deep;
    }
    if (embed && embed !== CFG.lmModelEmbed) {
      runtime.notes.push(`embed → ${embed}`);
      CFG.lmModelEmbed = embed;
    }
    if (!catalog.chat?.includes(CFG.modelFast)) {
      runtime.notes.push(`lm offline or model not loaded: ${CFG.modelFast}`);
    }
  } else {
    runtime.notes.push(`lm unreachable: ${catalog.error}`);
  }

  const embedReady = Boolean(CFG.lmModelEmbed && catalog.ok && catalog.embed?.includes(CFG.lmModelEmbed));
  const wantEmbeddings = CFG.memoryUseEmbeddings !== false;
  runtime.embeddings = wantEmbeddings && embedReady;
  CFG.memoryUseEmbeddingsEffective = runtime.embeddings;

  if (wantEmbeddings && embedReady && CFG.memoryUseEmbeddings !== true) {
    runtime.notes.push("memory: embeddings on");
  } else if (!embedReady && CFG.memoryUseEmbeddings !== false) {
    runtime.notes.push("memory: lexical (no embed model)");
  }

  runtime.bootstrapped = true;
  return runtimeSummary();
}

export function memoryEmbeddingsActive() {
  if (CFG.memoryUseEmbeddings === false) {
    return false;
  }
  return CFG.memoryUseEmbeddingsEffective ?? Boolean(CFG.lmModelEmbed);
}

export function runtimeSummary() {
  return {
    ...runtime,
    lm: {
      url: CFG.lmBaseUrl,
      fast: CFG.modelFast,
      deep: CFG.modelDeep,
      embed: CFG.lmModelEmbed,
    },
    stt: runtime.stt ?? {},
    embeddings: memoryEmbeddingsActive(),
  };
}

export function adaptivePipelineIntervalMs(pendingTotal = 0) {
  const base = CFG.pipelineIntervalMs ?? 30_000;
  if (pendingTotal <= 0) {
    return base;
  }
  if (pendingTotal > 100) {
    return Math.max(2000, Math.floor(base / 6));
  }
  if (pendingTotal > 20) {
    return Math.max(4000, Math.floor(base / 3));
  }
  return Math.max(8000, Math.floor(base / 2));
}

export function derivedPresentGaps() {
  const chunk = CFG.audioChunkMs ?? 1000;
  const frameMs = CFG.frameCaptureIntervalMs ?? 60_000;
  return {
    speechGapMs: CFG.presentSpeechGapMs ?? Math.max(30_000, chunk * 40),
    sceneGapMs: CFG.presentSceneGapMs ?? Math.max(120_000, frameMs * 6),
    soundGapMs: CFG.presentSoundGapMs ?? Math.max(15_000, chunk * 15),
  };
}

/** After config hot-reload from API/UI. */
export async function onConfigReloaded() {
  reloadConfig();
  return bootstrapRuntime({ force: true });
}
