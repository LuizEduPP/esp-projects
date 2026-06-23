import { CFG } from "../config/index.mjs";
import { promptLanguageRule } from "../locale/index.mjs";
import { ModelSlot } from "../models/index.mjs";
import { formatSceneCaption } from "./scene-caption.mjs";
import { parseVisionFallback } from "../util/json.mjs";
import { chatJsonLenientWithSlot } from "./client.mjs";

export async function captionFrame(b64, reason, ctx = {}) {
  const prev = ctx.previousScene;
  const prevCaption = ctx.previousCaption;
  let contextBlock = "";
  if (prev || prevCaption) {
    const desc = prev?.summary || prevCaption || formatSceneCaption(prev);
    contextBlock =
      `Last observation: "${desc}". ` +
      "If nothing meaningful changed (same room, people, activity), reply ONLY " +
      '{"unchanged":true}. ';
  }

  const prompt =
    contextBlock +
    "Home witness camera — describe like a calm observer, not a security log. " +
    "ONE raw JSON object, no markdown. Schema: " +
    '{"unchanged":false,"summary":"one natural sentence","person_present":bool,"people":0,' +
    '"scene":"short","activity":"what happens","objects":["up to 3"],"mood":"calm|focused|tense|social|empty","note":"optional detail"}. ' +
    "summary = how you'd tell a friend what you see. " +
    "person_present=true ONLY for a real human in the room — not photos/posters/TV. " +
    "Don't invent phones, selfies, or calls unless clearly visible. " +
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
  return scene;
}

export { formatSceneCaption } from "./scene-caption.mjs";
