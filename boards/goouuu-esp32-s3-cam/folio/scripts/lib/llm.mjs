import { CFG } from "./config.mjs";
import { promptLanguageRule } from "./locale.mjs";
import { modelId, ModelSlot } from "./models.mjs";
import { parseJsonLoose, parseVisionFallback } from "./util.mjs";


// --- openai-base.mjs ---
/** Normalize any legacy URL to OpenAI-compatible base: https://host/v1 */

const ENDPOINT_SUFFIX =
  /\/v1\/(?:chat\/completions|embeddings|models|rerank|audio\/transcriptions|images\/generations)\/?$/i;

export function normalizeOpenAiBase(url) {
  let base = String(url ?? "").trim().replace(/\/+$/, "");
  if (!base) {
    return "http://127.0.0.1:1234/v1";
  }
  base = base.replace(ENDPOINT_SUFFIX, "");
  if (!/\/v1$/i.test(base)) {
    base = `${base}/v1`;
  }
  return base;
}

export function joinOpenAiPath(baseUrl, endpoint) {
  const base = normalizeOpenAiBase(baseUrl);
  const path = String(endpoint).replace(/^\/+/, "");
  return `${base}/${path}`;
}

// --- openai.mjs ---

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

/** Resolve full URL for LM Studio (OpenAI-compatible) resource. */
export function openAiUrl(resource) {
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
    throw new Error(`LM Studio ${res.status}: ${text.slice(0, 240)}`);
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

// --- models-catalog.mjs ---
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
      error: err.message ?? "LM Studio unreachable",
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
    model: model ?? CFG.lmModelRerank,
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

// --- client.mjs ---
export async function chatCompletion({
  model,
  messages,
  temperature = 0.2,
  maxTokens = CFG.lmChatMaxTokens,
  responseFormat,
}) {
  const body = {
    model,
    messages,
    temperature,
    max_tokens: maxTokens,
  };
  if (responseFormat) {
    body.response_format = responseFormat;
  }
  const json = await chatCompletions(body);
  return messageContent(json?.choices?.[0]?.message ?? {});
}

export async function chatJson({
  model,
  messages,
  temperature = 0.1,
  maxTokens = CFG.lmChatMaxTokensDeep,
  responseFormat,
}) {
  const text = await chatCompletion({ model, messages, temperature, maxTokens, responseFormat });
  const parsed = parseJsonLoose(text);
  if (!parsed) {
    throw new Error(`Model returned non-JSON: ${text.slice(0, 200)}`);
  }
  return parsed;
}

export async function chatJsonLenient({
  model,
  messages,
  temperature = 0.1,
  maxTokens = CFG.lmChatMaxTokensDeep,
  fallback,
  responseFormat,
}) {
  let text;
  try {
    text = await chatCompletion({ model, messages, temperature, maxTokens, responseFormat });
  } catch (err) {
    const msg = String(err?.message ?? err);
    if (responseFormat && /response_format|json_object|json_schema/i.test(msg)) {
      text = await chatCompletion({ model, messages, temperature, maxTokens });
    } else {
      throw err;
    }
  }
  const parsed =
    parseJsonLoose(text) ?? (typeof fallback === "function" ? fallback(text) : null);
  if (!parsed) {
    throw new Error(`Model returned non-JSON: ${text.slice(0, 200)}`);
  }
  return parsed;
}

export async function chatWithSlot(slot, opts) {
  return chatCompletion({ ...opts, model: modelId(slot) });
}

export async function chatJsonWithSlot(slot, opts) {
  return chatJson({ ...opts, model: modelId(slot) });
}

export async function chatJsonLenientWithSlot(slot, opts) {
  return chatJsonLenient({ ...opts, model: modelId(slot) });
}

export { ModelSlot };

// --- scene.mjs ---
export function formatSceneCaption(scene) {
  if (!scene || typeof scene !== "object") {
    return "";
  }
  if (scene.summary && String(scene.summary).trim()) {
    return String(scene.summary).trim();
  }
  if (scene.unchanged) {
    return "";
  }
  const activity = scene.activity?.trim();
  const sceneLabel = scene.scene?.trim();
  const note = scene.note?.trim();
  if (scene.person_present && activity) {
    const who = Number(scene.people) > 1 ? `${scene.people} pessoas` : "Alguém";
    return note ? `${who} ${activity.toLowerCase()} — ${note}` : `${who} ${activity.toLowerCase()}.`;
  }
  if (activity && activity !== "unknown" && !/^nothing|empty|nada|vazio/i.test(activity)) {
    return note ? `${activity}. ${note}` : `${activity}.`;
  }
  if (sceneLabel && sceneLabel !== "unknown") {
    return note ? `${sceneLabel}. ${note}` : `${sceneLabel}.`;
  }
  return note || "Ambiente quieto.";
}

export function sceneFingerprint(scene) {
  if (!scene || scene.unchanged) {
    return "unchanged";
  }
  return [
    scene.person_present ? "p" : "-",
    scene.people ?? 0,
    norm(scene.scene),
    norm(scene.activity),
    scene.mood ?? "",
  ].join("|");
}

function norm(s) {
  return String(s ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^\p{L}\p{N}\s]/gu, " ")
    .replace(/\s+/g, " ")
    .trim()
    .slice(0, 48);
}

export async function captionFrame(b64, reason, ctx = {}) {
  const prev = ctx.previousScene;
  const prevCaption = ctx.previousCaption;
  const quality = ctx.quality;
  const motion = ctx.motion;

  let contextBlock = "";
  if (prev || prevCaption) {
    const desc = prev?.summary || prevCaption || formatSceneCaption(prev);
    contextBlock =
      `Last observation: "${desc}". ` +
      'If nothing meaningful changed, reply ONLY {"unchanged":true}. ';
  }

  let sceneHints = "";
  if (quality?.nearlyBlack) {
    sceneHints +=
      "Image is nearly black — reply unchanged OR summary admitting visibility is too low. " +
      "NEVER invent people, phones, beds, laptops, or furniture you cannot clearly see. ";
  } else if (quality?.dark) {
    sceneHints +=
      "Image is underexposed (auto-enhanced). Only describe clearly visible shapes. " +
      "If unsure about people or objects, set person_present:false and people:0. ";
  } else if (ctx.vision?.enhanced) {
    sceneHints += "Image was auto-enhanced for visibility. ";
  }
  if (quality?.bright) {
    sceneHints += "Image is overexposed — infer from silhouettes only; do not invent details. ";
  }
  if (motion?.level === "high") {
    sceneHints += "Significant motion detected. ";
  }

  const prompt =
    contextBlock +
    sceneHints +
    "Home witness camera. ONE raw JSON, no markdown. Schema: " +
    '{"unchanged":false,"summary":"one natural sentence","person_present":bool,"people":0,' +
    '"scene":"room label","activity":"what is happening","objects":[{"name":"object","count":1}],' +
    '"tags":["indoor|outdoor|pet|…"],"mood":"calm|focused|tense|social|empty","note":"optional"}. ' +
    "Identify visible objects only when clearly seen. " +
    "summary = one honest sentence; prefer unchanged if the scene matches the last observation. " +
    "Real humans only — not photos on walls, not guesses in darkness. " +
    `Trigger: ${reason || "interval"}. ` +
    promptLanguageRule();

  const scene = await chatJsonLenientWithSlot(ModelSlot.FAST, {
    temperature: CFG.frameCaptionTemperature,
    maxTokens: CFG.frameCaptionMaxTokens,
    responseFormat: { type: "json_object" },
    fallback: parseVisionFallback,
    messages: [
      {
        role: "user",
        content: [
          { type: "image_url", image_url: { url: `data:image/jpeg;base64,${b64}` } },
          { type: "text", text: prompt },
        ],
      },
    ],
  });

  if (scene?.unchanged && prev) {
    return { ...prev, unchanged: true };
  }

  if (!scene.summary) {
    scene.summary = formatSceneCaption(scene);
  }
  if (Array.isArray(scene.objects)) {
    scene.objects = scene.objects
      .map((o) => {
        if (typeof o === "string") {
          return { name: o, count: 1 };
        }
        if (!o || typeof o !== "object") {
          return null;
        }
        const count = o.count ?? o["count:"] ?? 1;
        const name = String(o.name ?? o.nome ?? "").trim();
        if (!name) {
          return null;
        }
        return { name, count: Number(count) || 1 };
      })
      .filter(Boolean);
  }
  return scene;
}
