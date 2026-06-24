/**
 * Model slot routing — maps logical roles to configured model IDs (LM Studio).
 */
import { CFG } from "./config.mjs";
import { openAiUrl } from "./llm.mjs";

export const ModelSlot = Object.freeze({
  FAST: "fast",
  DEEP: "deep",
  EMBED: "embed",
  RERANK: "rerank",
});

export function modelId(slot) {
  switch (slot) {
    case ModelSlot.FAST:
      return CFG.modelFast;
    case ModelSlot.DEEP:
      return CFG.modelDeep;
    case ModelSlot.EMBED:
      return CFG.lmModelEmbed;
    case ModelSlot.RERANK:
      return CFG.lmModelRerank;
    default:
      throw new Error(`Unknown model slot: ${slot}`);
  }
}

export function lmChatUrl() {
  return openAiUrl("chat");
}

export function embeddingsUrl() {
  return openAiUrl("embeddings");
}

export function modelSummary() {
  return {
    fast: CFG.modelFast,
    deep: CFG.modelDeep,
    embed: CFG.lmModelEmbed,
    rerank: CFG.lmModelRerank,
    lmBaseUrl: CFG.lmBaseUrl,
  };
}
