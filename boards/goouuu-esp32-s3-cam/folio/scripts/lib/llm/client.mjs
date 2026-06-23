import { modelId, ModelSlot } from "../models/index.mjs";
import { parseJsonLoose } from "../util/json.mjs";
import { chatCompletions, messageContent } from "./openai.mjs";

export async function chatCompletion({
  model,
  messages,
  temperature = 0.2,
  maxTokens = 2048,
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
  maxTokens = 4096,
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
  maxTokens = 4096,
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
