import { existsSync, readFileSync } from "node:fs";
import jpeg from "jpeg-js";
import { CFG } from "../config.mjs";
import {
  insertEvent,
  lastProcessedFrame,
  markFrameProcessed,
} from "../db.mjs";
import { captionFrame, formatSceneCaption } from "../llm.mjs";

const MotionLevel = Object.freeze({ NONE: "none", LOW: "low", HIGH: "high" });
const GREY_SAMPLES = 32;

function msBetween(earlierIso, laterIso) {
  if (!earlierIso || !laterIso) {
    return Infinity;
  }
  return new Date(laterIso).getTime() - new Date(earlierIso).getTime();
}

function greyFromRgba(data, width, height, outW, outH) {
  const out = new Uint8Array(outW * outH);
  const xStep = width / outW;
  const yStep = height / outH;
  for (let y = 0; y < outH; y++) {
    for (let x = 0; x < outW; x++) {
      const sx = Math.min(width - 1, Math.floor(x * xStep));
      const sy = Math.min(height - 1, Math.floor(y * yStep));
      const i = (sy * width + sx) * 4;
      out[y * outW + x] = Math.round(
        0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2],
      );
    }
  }
  return out;
}

function decodeJpeg(buf) {
  return jpeg.decode(buf, { useTArray: true, formatAsRGBA: true });
}

function jpegQuality(buf) {
  try {
    const { data, width, height } = decodeJpeg(buf);
    if (!data?.length || !width || !height) {
      return { brightness: 0.5, width: 0, height: 0, dark: false, bright: false };
    }
    let sum = 0;
    const pixels = width * height;
    for (let i = 0; i < data.length; i += 4) {
      sum += 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
    }
    const brightness = sum / pixels / 255;
    return {
      brightness,
      width,
      height,
      dark: brightness < 0.28,
      bright: brightness > 0.82,
    };
  } catch {
    return { brightness: 0.5, width: 0, height: 0, dark: false, bright: false };
  }
}

function greyThumb(buf, size = GREY_SAMPLES) {
  const { data, width, height } = decodeJpeg(buf);
  return greyFromRgba(data, width, height, size, size);
}

function motionScore(bufA, bufB, size = GREY_SAMPLES) {
  try {
    const a = greyThumb(bufA, size);
    const b = greyThumb(bufB, size);
    let diff = 0;
    for (let i = 0; i < a.length; i++) {
      diff += Math.abs(a[i] - b[i]);
    }
    return diff / (a.length * 255);
  } catch {
    return 1;
  }
}

function compareFrames(prevBuf, nextBuf) {
  const score = motionScore(prevBuf, nextBuf);
  return {
    score,
    changed: score >= 0.035,
    level: score >= 0.12 ? MotionLevel.HIGH : score >= 0.035 ? MotionLevel.LOW : MotionLevel.NONE,
  };
}

function clamp(v) {
  return Math.max(0, Math.min(255, Math.round(v)));
}

function enhanceJpeg(buf, { target = 0.42, maxGain = 2.2 } = {}) {
  try {
    const decoded = decodeJpeg(buf);
    const { data, width, height } = decoded;
    const q = jpegQuality(buf);
    if (!q.dark && !q.bright) {
      return buf;
    }

    let gain = 1;
    if (q.dark) {
      gain = Math.min(maxGain, target / Math.max(q.brightness, 0.06));
    } else if (q.bright) {
      gain = Math.max(0.65, target / q.brightness);
    }

    for (let i = 0; i < data.length; i += 4) {
      data[i] = clamp(data[i] * gain);
      data[i + 1] = clamp(data[i + 1] * gain);
      data[i + 2] = clamp(data[i + 2] * gain);
    }

    return Buffer.from(jpeg.encode({ data, width, height }, 82).data);
  } catch {
    return buf;
  }
}

function enrichScene(scene, { motion, quality } = {}) {
  const out = { ...scene };
  if (motion) {
    out.motion_score = motion.score;
    out.motion_level = motion.level ?? MotionLevel.NONE;
  }
  if (quality) {
    out.brightness = quality.brightness;
    out.lighting = quality.dark ? "dark" : quality.bright ? "bright" : "normal";
    if (quality.dark) {
      out.enhanced = true;
    }
  }
  if (Array.isArray(out.objects)) {
    out.object_tags = out.objects
      .map((o) => (typeof o === "string" ? o : o?.name))
      .filter(Boolean)
      .slice(0, 6);
  }
  return out;
}

function objectsSummary(scene) {
  const tags = scene?.object_tags ?? scene?.objects ?? [];
  if (!tags.length) {
    return null;
  }
  return tags
    .map((o) => (typeof o === "string" ? o : o?.name))
    .filter(Boolean)
    .slice(0, 4)
    .join(", ");
}

function shouldAnalyze({ motion, frame, prev, forceIntervalMs }) {
  const gapMs = msBetween(prev?.captured_at, frame.captured_at);
  if (motion.level === MotionLevel.HIGH) {
    return true;
  }
  if (motion.level === MotionLevel.LOW && gapMs >= forceIntervalMs / 2) {
    return true;
  }
  if (gapMs >= forceIntervalMs) {
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
  return { id: frame.id, skipped: "no_motion", motion: motion.score, usedLm: false };
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
    usedLm: true,
  };
}
