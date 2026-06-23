import { CFG } from "../config/index.mjs";
import { promptLanguageRule } from "../locale/index.mjs";
import { ModelSlot } from "../models/index.mjs";
import { chatJsonWithSlot } from "./client.mjs";

export async function captionFrame(b64, reason) {
  const prompt =
    "Room witness camera frame. Reply raw JSON only, no markdown: " +
    '{"person_present":bool,"people":0,"scene":"short label","activity":"what is happening","objects":["up to 4"],"mood":"calm|focused|tense|social|empty","note":"max 20 words"}. ' +
    "person_present=true ONLY for a real human (face/body). false for empty room. " +
    `Trigger: ${reason || "interval"}. Describe only visible facts. ` +
    promptLanguageRule();

  return chatJsonWithSlot(ModelSlot.FAST, {
    temperature: CFG.frameCaptionTemperature,
    maxTokens: CFG.frameCaptionMaxTokens,
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
