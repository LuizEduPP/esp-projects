import { CFG } from "./config.mjs";
import { promptLanguageRule } from "./locale.mjs";
import { parseJsonLoose } from "./util.mjs";

export async function chatCompletion({ model, messages, temperature = 0.2, maxTokens = 2048 }) {
  const res = await fetch(CFG.lmUrl, {
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

export async function captionFrame(b64, reason) {
  const prompt =
    "Room witness camera frame. Reply raw JSON only, no markdown: " +
    '{"person_present":bool,"people":0,"scene":"short label","activity":"what is happening","objects":["up to 4"],"mood":"calm|focused|tense|social|empty","note":"max 20 words"}. ' +
    "person_present=true ONLY for a real human (face/body). false for empty room. " +
    `Trigger: ${reason || "interval"}. Describe only visible facts. ` +
    promptLanguageRule();

  return chatJson({
    model: CFG.modelFast,
    temperature: 0.05,
    maxTokens: 220,
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
}
