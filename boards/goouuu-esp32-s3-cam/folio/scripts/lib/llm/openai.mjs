import { CFG } from "../config/index.mjs";
import { joinOpenAiPath, normalizeOpenAiBase } from "./openai-base.mjs";

export { normalizeOpenAiBase, joinOpenAiPath };

const RESOURCE_PATH = {
  chat: "chat/completions",
  embeddings: "embeddings",
  models: "models",
  rerank: "rerank",
};

export function openAiBaseUrl() {
  return CFG.openaiBaseUrl;
}

export function openAiHeaders(extra = {}) {
  const headers = { "Content-Type": "application/json", ...extra };
  if (CFG.openaiApiKey) {
    headers.Authorization = `Bearer ${CFG.openaiApiKey}`;
  }
  return headers;
}

/** Resolve full URL for an OpenAI-compatible resource. */
export function openAiUrl(resource) {
  if (resource === "embeddings" && CFG.memoryEmbeddingsUrl) {
    return CFG.memoryEmbeddingsUrl;
  }
  if (resource === "rerank" && CFG.memoryRerankUrl) {
    return CFG.memoryRerankUrl;
  }
  const path = RESOURCE_PATH[resource] ?? String(resource).replace(/^\/+/, "");
  return joinOpenAiPath(openAiBaseUrl(), path);
}

export async function openAiRequest(url, { method = "POST", body, timeoutMs = 180_000 } = {}) {
  const res = await fetch(url, {
    method,
    headers: openAiHeaders(),
    body: body != null ? JSON.stringify(body) : undefined,
    signal: AbortSignal.timeout(timeoutMs),
  });

  if (!res.ok) {
    const text = await res.text();
    throw new Error(`OpenAI API ${res.status}: ${text.slice(0, 240)}`);
  }

  return res.json();
}

export async function openAiResource(resource, opts = {}) {
  return openAiRequest(openAiUrl(resource), opts);
}

/** POST /v1/chat/completions — standard OpenAI shape. */
export async function chatCompletions(body, { timeoutMs = 300_000 } = {}) {
  return openAiRequest(openAiUrl("chat"), { method: "POST", body, timeoutMs });
}

/** POST /v1/embeddings */
export async function createEmbeddings(body, { timeoutMs = 60_000 } = {}) {
  return openAiRequest(openAiUrl("embeddings"), { method: "POST", body, timeoutMs });
}

/** GET /v1/models */
export async function listModels({ timeoutMs = 8_000 } = {}) {
  return openAiRequest(openAiUrl("models"), { method: "GET", timeoutMs });
}

/** POST /v1/rerank — Jina/TEI extension (not core OpenAI). */
export async function rerank(body, { timeoutMs = 60_000 } = {}) {
  return openAiRequest(openAiUrl("rerank"), { method: "POST", body, timeoutMs });
}

export function messageContent(msg) {
  const content = msg?.content ?? msg?.reasoning_content;
  if (typeof content === "string") {
    return content.trim();
  }
  if (Array.isArray(content)) {
    return content
      .filter((p) => p.type === "text")
      .map((p) => p.text)
      .join("")
      .trim();
  }
  return "";
}
