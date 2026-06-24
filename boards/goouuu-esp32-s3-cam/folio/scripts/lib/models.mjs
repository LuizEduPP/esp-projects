/**
 * Model slot routing — maps logical roles to configured model IDs.
 */
import { CFG } from "./config.mjs";
import { openAiUrl } from "./llm.mjs";

export const ModelSlot = Object.freeze({
  FAST: "fast",
  DEEP: "deep",
  WHISPER: "whisper",
  EMBED: "embed",
  RERANK: "rerank",
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
      return CFG.lmModelEmbed;
    case ModelSlot.RERANK:
      return CFG.lmModelRerank;
    default:
      throw new Error(`Unknown model slot: ${slot}`);
  }
}

/** @deprecated use openAiUrl("chat") */
export function lmChatUrl() {
  return openAiUrl("chat");
}

/** @deprecated use openAiUrl("embeddings") */
export function embeddingsUrl() {
  return openAiUrl("embeddings");
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
    embed: CFG.lmModelEmbed,
    rerank: CFG.lmModelRerank,
    whisperDevice: CFG.whisperDevice,
    lmBaseUrl: CFG.lmBaseUrl,
  };
}
