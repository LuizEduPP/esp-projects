import { CFG } from "../config/index.mjs";
import { createEmbeddings } from "../llm/openai.mjs";
import { modelId, ModelSlot } from "../models/index.mjs";
import { cosineDense, cosineSimilarity, termVector, vectorFromJson } from "./lexical.mjs";

export async function embedText(text) {
  if (!CFG.memoryUseEmbeddings) {
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }
  const json = await createEmbeddings({
    model: modelId(ModelSlot.EMBED),
    input: text.slice(0, 8192),
    encoding_format: "float",
  });
  const vec = json?.data?.[0]?.embedding;
  if (!Array.isArray(vec)) {
    throw new Error("no embedding vector");
  }
  return { kind: "float", vector: vec };
}

export function scorePair(queryEmbed, docEmbedJson) {
  const stored = vectorFromJson(docEmbedJson);

  if (queryEmbed.kind === "float") {
    return Array.isArray(stored) ? cosineDense(queryEmbed.vector, stored) : 0;
  }

  if (stored instanceof Map) {
    return cosineSimilarity(new Map(queryEmbed.vector), stored);
  }

  return 0;
}

export function serializeEmbedding(embed) {
  return JSON.stringify(embed.vector);
}
