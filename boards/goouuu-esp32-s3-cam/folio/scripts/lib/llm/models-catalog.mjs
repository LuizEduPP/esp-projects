import { CFG } from "../config/index.mjs";
import { listModels, openAiBaseUrl, rerank } from "./openai.mjs";

function modelIdFromEntry(entry) {
  return entry?.id ?? entry?.model ?? entry?.name ?? null;
}

function classifyModel(id) {
  const lower = String(id).toLowerCase();
  if (/rerank|re-rank|cross-encoder/.test(lower)) {
    return "rerank";
  }
  if (/embed|nomic|bge-m3|e5-|minilm|text-embedding/.test(lower) && !/rerank/.test(lower)) {
    return "embed";
  }
  return "chat";
}

/** GET /v1/models — OpenAI-compatible model list. */
export async function fetchOpenAiModels() {
  try {
    const json = await listModels();
    const raw = json?.data ?? json?.models ?? json;
    const list = Array.isArray(raw) ? raw : [];
    const ids = [...new Set(list.map(modelIdFromEntry).filter(Boolean))];

    const chat = [];
    const embed = [];
    const rerankModels = [];
    for (const id of ids) {
      const kind = classifyModel(id);
      if (kind === "rerank") {
        rerankModels.push(id);
      } else if (kind === "embed") {
        embed.push(id);
      } else {
        chat.push(id);
      }
    }

    return {
      ok: true,
      baseUrl: openAiBaseUrl(),
      chat,
      embed,
      rerank: rerankModels,
      all: ids,
    };
  } catch (err) {
    return {
      ok: false,
      error: err.message ?? "OpenAI API unreachable",
      baseUrl: openAiBaseUrl(),
    };
  }
}

/** @deprecated use fetchOpenAiModels */
export const fetchLmModels = fetchOpenAiModels;

/** POST /v1/rerank — Jina / TEI extension. */
export async function rerankDocuments(query, documents, { model, topN } = {}) {
  if (!documents.length) {
    return [];
  }

  const json = await rerank({
    model: model ?? CFG.memoryRerankModel,
    query,
    documents,
    top_n: topN ?? documents.length,
  });

  const results = json?.results ?? json?.data ?? [];
  return results
    .map((r) => ({
      index: r.index,
      score: r.relevance_score ?? r.score ?? 0,
    }))
    .sort((a, b) => b.score - a.score);
}
