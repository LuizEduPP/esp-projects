import { lmChatUrl, modelId, ModelSlot } from "../models/index.mjs";
import { parseJsonLoose } from "../util/json.mjs";

export async function chatCompletion({ model, messages, temperature = 0.2, maxTokens = 2048 }) {
  const res = await fetch(lmChatUrl(), {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({
      model,
      temperature,
      max_tokens: maxTokens,
      messages,
    }),
    signal: AbortSignal.timeout(180000),
  });
  if (!res.ok) {
    const t = await res.text();
    throw new Error(`LM Studio ${res.status}: ${t.slice(0, 240)}`);
  }
  const json = await res.json();
  const msg = json?.choices?.[0]?.message ?? {};
  return (msg.content || msg.reasoning_content || "").trim();
}

export async function chatJson({ model, messages, temperature = 0.1, maxTokens = 4096 }) {
  const text = await chatCompletion({ model, messages, temperature, maxTokens });
  const parsed = parseJsonLoose(text);
  if (!parsed) {
    throw new Error(`Model returned non-JSON: ${text.slice(0, 200)}`);
  }
  return parsed;
}

export async function chatJsonLenient({ model, messages, temperature = 0.1, maxTokens = 4096, fallback }) {
  const text = await chatCompletion({ model, messages, temperature, maxTokens });
  const parsed = parseJsonLoose(text) ?? (typeof fallback === "function" ? fallback(text) : null);
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
