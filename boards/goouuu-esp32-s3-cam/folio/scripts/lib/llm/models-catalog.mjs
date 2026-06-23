import { CFG } from "../config/index.mjs";

/** Base URL from lm.url (strip /v1/chat/completions). */
export function lmBaseUrl() {
  return CFG.lmUrl.replace(/\/v1\/chat\/completions\/?$/i, "").replace(/\/$/, "");
}

export function rerankUrl() {
  if (CFG.memoryRerankUrl) {
    return CFG.memoryRerankUrl;
  }
  return `${lmBaseUrl()}/v1/rerank`;
}

export function modelsListUrl() {
  return `${lmBaseUrl()}/v1/models`;
}

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

async function fetchJson(url, timeoutMs = 8000) {
  const res = await fetch(url, { signal: AbortSignal.timeout(timeoutMs) });
  if (!res.ok) {
    throw new Error(`${url} → ${res.status}`);
  }
  return res.json();
}

/** List models from LM Studio (OpenAI-compatible /v1/models). */
export async function fetchLmModels() {
  const urls = [modelsListUrl(), `${lmBaseUrl()}/api/v1/models`];
  let lastErr = null;

  for (const url of urls) {
    try {
      const json = await fetchJson(url);
      const raw = json?.data ?? json?.models ?? json;
      const list = Array.isArray(raw) ? raw : [];
      const ids = [...new Set(list.map(modelIdFromEntry).filter(Boolean))];

      const chat = [];
      const embed = [];
      const rerank = [];
      for (const id of ids) {
        const kind = classifyModel(id);
        if (kind === "rerank") {
          rerank.push(id);
        } else if (kind === "embed") {
          embed.push(id);
        } else {
          chat.push(id);
        }
      }

      return {
        ok: true,
        source: url,
        baseUrl: lmBaseUrl(),
        chat,
        embed,
        rerank,
        all: ids,
      };
    } catch (err) {
      lastErr = err;
    }
  }

  return { ok: false, error: lastErr?.message ?? "LM Studio unreachable", baseUrl: lmBaseUrl() };
}

/** Jina / TEI-style rerank API. */
export async function rerankDocuments(query, documents, { model, topN } = {}) {
  if (!documents.length) {
    return [];
  }

  const res = await fetch(rerankUrl(), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model: model ?? CFG.memoryRerankModel,
      query,
      documents,
      top_n: topN ?? documents.length,
    }),
    signal: AbortSignal.timeout(60_000),
  });

  if (!res.ok) {
    const t = await res.text();
    throw new Error(`rerank ${res.status}: ${t.slice(0, 200)}`);
  }

  const json = await res.json();
  const results = json?.results ?? json?.data ?? [];
  return results
    .map((r) => ({
      index: r.index,
      score: r.relevance_score ?? r.score ?? 0,
    }))
    .sort((a, b) => b.score - a.score);
}
