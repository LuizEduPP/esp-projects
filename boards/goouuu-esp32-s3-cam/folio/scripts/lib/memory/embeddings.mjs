import { CFG } from "../config/index.mjs";
import { createEmbeddings } from "../llm/openai.mjs";
import { modelId, ModelSlot } from "../models/index.mjs";
import { cosineDense, cosineSimilarity, termVector, vectorFromJson } from "./lexical.mjs";

export async function embedText(text) {
  if (!CFG.memoryUseEmbeddings) {
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }
  try {
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
  } catch (err) {
    if (!CFG.memoryFallbackLexical) {
      throw err;
    }
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }
}

export function scorePair(queryEmbed, docEmbedJson, docText) {
  const stored = vectorFromJson(docEmbedJson);

  if (queryEmbed.kind === "float" && Array.isArray(stored)) {
    return cosineDense(queryEmbed.vector, stored);
  }

  const queryMap =
    queryEmbed.kind === "lexical" ? new Map(queryEmbed.vector) : termVector(docText);
  const docMap = stored instanceof Map ? stored : termVector(docText);
  return cosineSimilarity(queryMap, docMap);
}

export function serializeEmbedding(embed) {
  return JSON.stringify(embed.vector);
}
