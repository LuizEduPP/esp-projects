import { CFG } from "../config/index.mjs";
import { promptLanguageRule } from "../locale/index.mjs";
import { ModelSlot } from "../models/index.mjs";
import { parseVisionFallback } from "../util.mjs";
import { chatJsonLenientWithSlot } from "./client.mjs";

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
  if (quality?.dark) {
    sceneHints += "Image is underexposed — infer carefully from visible shapes. ";
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
    "Identify visible objects (furniture, pets, devices, people). " +
    "summary = tell a friend what you see. Real humans only — not photos on walls. " +
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
  if (Array.isArray(scene.objects) && scene.objects.every((o) => typeof o === "string")) {
    scene.objects = scene.objects.map((name) => ({ name, count: 1 }));
  }
  return scene;
}
