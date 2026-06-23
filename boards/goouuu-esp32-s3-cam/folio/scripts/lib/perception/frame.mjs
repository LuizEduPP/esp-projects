import { existsSync, readFileSync } from "node:fs";
import { CFG } from "../config/index.mjs";
import {
  insertEvent,
  lastProcessedFrame,
  markFrameProcessed,
} from "../db/index.mjs";
import { captionFrame, formatSceneCaption } from "../llm/scene.mjs";
import { compareFrames, enhanceJpeg, jpegQuality } from "./image.mjs";
import { enrichScene, objectsSummary } from "./scene.mjs";
import { MotionLevel } from "./types.mjs";

function msSince(iso) {
  if (!iso) {
    return Infinity;
  }
  return Date.now() - new Date(iso).getTime();
}

function shouldAnalyze({ motion, frame, prev, forceIntervalMs }) {
  if (frame.reason === "motion") {
    return true;
  }
  if (motion.level === MotionLevel.HIGH) {
    return true;
  }
  if (motion.level === MotionLevel.LOW && msSince(prev?.captured_at) >= forceIntervalMs / 2) {
    return true;
  }
  if (msSince(prev?.captured_at) >= forceIntervalMs) {
    return true;
  }
  return motion.changed && motion.score >= CFG.perceptionMotionMin;
}

function markSkipped(db, frame, motion, quality) {
  const scene = enrichScene(
    {
      unchanged: true,
      summary: CFG.frameStaticSummary,
      person_present: false,
      people: 0,
      scene: "static",
      activity: "quiet",
      objects: [],
      mood: "empty",
    },
    { motion, quality },
  );
  markFrameProcessed(db, frame.id, scene.summary, JSON.stringify(scene));
  return { id: frame.id, skipped: "no_motion", motion: motion.score };
}

export async function processFrame(db, frame) {
  const buf = readFileSync(frame.path);
  const quality = jpegQuality(buf);

  let motion = { score: 1, changed: true, level: MotionLevel.HIGH };
  const prev = lastProcessedFrame(db);
  if (prev?.path && existsSync(prev.path)) {
    motion = compareFrames(readFileSync(prev.path), buf);
  }

  if (!shouldAnalyze({ motion, frame, prev, forceIntervalMs: CFG.perceptionMotionForceMs })) {
    return markSkipped(db, frame, motion, quality);
  }

  let visionBuf = buf;
  if (CFG.perceptionAutoEnhance && quality.dark) {
    visionBuf = enhanceJpeg(buf);
  }

  let previousScene = null;
  if (prev?.scene_json) {
    try {
      previousScene = JSON.parse(prev.scene_json);
    } catch {
      /* ignore */
    }
  }

  const sceneRaw = await captionFrame(visionBuf.toString("base64"), frame.reason, {
    previousCaption: prev?.caption,
    previousScene,
    quality,
    motion,
  });

  const scene = enrichScene(sceneRaw, { motion, quality });
  const caption = scene.unchanged && prev?.caption ? prev.caption : formatSceneCaption(scene);
  const tags = objectsSummary(scene);
  const finalCaption = tags && !caption.includes(tags) ? `${caption} (${tags})` : caption;

  markFrameProcessed(db, frame.id, finalCaption, JSON.stringify(scene));

  const people = scene.unchanged
    ? Number(previousScene?.people ?? 0)
    : Number(scene.people) || 0;
  const personPresent = scene.unchanged
    ? previousScene?.person_present === true
    : scene.person_present === true;

  if (personPresent || people > 0) {
    insertEvent(db, {
      device_id: frame.device_id,
      at: frame.captured_at,
      kind: "presence",
      payload_json: JSON.stringify({ source: "camera", people, frame_id: frame.id }),
    });
  }

  if (motion.level !== MotionLevel.NONE) {
    insertEvent(db, {
      device_id: frame.device_id,
      at: frame.captured_at,
      kind: "motion",
      payload_json: JSON.stringify({
        score: motion.score,
        level: motion.level,
        frame_id: frame.id,
      }),
    });
  }

  return {
    id: frame.id,
    caption: finalCaption.slice(0, 100),
    motion: motion.score,
    lighting: scene.lighting,
    unchanged: !!scene.unchanged,
  };
}
