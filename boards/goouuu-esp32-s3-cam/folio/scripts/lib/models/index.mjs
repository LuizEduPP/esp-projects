/**
 * Single source of truth for AI model slots (S — one responsibility: model routing).
 */
import { CFG } from "../config/index.mjs";

export const ModelSlot = Object.freeze({
  FAST: "fast",
  DEEP: "deep",
  WHISPER: "whisper",
  EMBED: "embed",
});

export function modelId(slot) {
  switch (slot) {
    case ModelSlot.FAST:
      return CFG.modelFast;
    case ModelSlot.DEEP:
      return CFG.modelDeep;
    case ModelSlot.WHISPER:
      return CFG.whisperModel;
    case ModelSlot.EMBED:
      return CFG.memoryEmbeddingModel || CFG.modelFast;
    default:
      throw new Error(`Unknown model slot: ${slot}`);
  }
}

export function lmChatUrl() {
  return CFG.lmUrl;
}

export function embeddingsUrl() {
  if (CFG.memoryEmbeddingsUrl) {
    return CFG.memoryEmbeddingsUrl;
  }
  return CFG.lmUrl.replace(/\/chat\/completions\/?$/, "/embeddings");
}

export function whisperRuntime() {
  return {
    bin: CFG.whisperBin,
    device: CFG.whisperDevice,
    timeoutMs: CFG.whisperTimeoutMs,
    language: CFG.whisperLanguage,
  };
}

export function modelSummary() {
  return {
    fast: CFG.modelFast,
    deep: CFG.modelDeep,
    whisper: CFG.whisperModel,
    embed: CFG.memoryEmbeddingModel || CFG.modelFast,
    whisperDevice: CFG.whisperDevice,
  };
}
