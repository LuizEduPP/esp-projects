import { CFG } from "../config.mjs";
import { cosineDense, cosineSimilarity, termVector } from "./lexical.mjs";

function embeddingsUrl() {
  if (CFG.memoryEmbeddingsUrl) {
    return CFG.memoryEmbeddingsUrl;
  }
  return CFG.lmUrl.replace(/\/chat\/completions\/?$/, "/embeddings");
}

export function vectorFromJson(raw) {
  if (!raw) {
    return null;
  }
  try {
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed) && typeof parsed[0] === "number") {
      return parsed;
    }
    if (Array.isArray(parsed) && Array.isArray(parsed[0])) {
      return new Map(parsed);
    }
    return null;
  } catch {
    return null;
  }
}

export async function embedText(text) {
  if (!CFG.memoryUseEmbeddings) {
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }

  try {
    const res = await fetch(embeddingsUrl(), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        model: CFG.memoryEmbeddingModel || CFG.modelFast,
        input: text.slice(0, 2000),
      }),
      signal: AbortSignal.timeout(60_000),
    });
    if (!res.ok) {
      throw new Error(`embeddings ${res.status}`);
    }
    const json = await res.json();
    const vec = json?.data?.[0]?.embedding;
    if (!Array.isArray(vec)) {
      throw new Error("no embedding vector");
    }
    return { kind: "float", vector: vec };
  } catch (err) {
    if (!CFG.memoryFallbackLexical) {
      throw new Error(`embeddings failed: ${err.message}`);
    }
    console.warn(`[memory] embeddings failed, fallback lexical: ${err.message}`);
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
