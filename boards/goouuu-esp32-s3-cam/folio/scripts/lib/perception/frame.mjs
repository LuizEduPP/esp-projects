import { existsSync, readFileSync } from "node:fs";
import jpeg from "jpeg-js";
import { CFG } from "../config.mjs";
import {
  insertEvent,
  lastProcessedFrame,
  markFrameProcessed,
} from "../db.mjs";
import { captionFrame, formatSceneCaption } from "../llm.mjs";
import { isDarkSceneCaption } from "../util.mjs";

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

function compareFrames(prevBuf, nextBuf, { dark = false } = {}) {
  const score = motionScore(prevBuf, nextBuf);
  const minChanged = dark ? 0.06 : 0.035;
  const minHigh = dark ? 0.18 : 0.12;
  return {
    score,
    changed: score >= minChanged,
    level: score >= minHigh ? MotionLevel.HIGH : score >= minChanged ? MotionLevel.LOW : MotionLevel.NONE,
  };
}

function clamp(v) {
  return Math.max(0, Math.min(255, Math.round(v)));
}

function lumaAt(data, i) {
  return 0.299 * data[i] + 0.587 * data[i + 1] + 0.114 * data[i + 2];
}

function visionOpts() {
  return CFG.perceptionVision ?? {};
}

function analyzeRgba(data, width, height, v = visionOpts()) {
  const pixels = width * height;
  if (!data?.length || !pixels) {
    return {
      brightness: 0.5,
      width: 0,
      height: 0,
      spread: 0,
      contrast: 0,
      dark: false,
      bright: false,
      flat: false,
      nearlyBlack: false,
      pLow: 0,
      pHigh: 255,
    };
  }

  const lumas = new Uint8Array(pixels);
  let sum = 0;
  for (let p = 0, i = 0; p < pixels; p++, i += 4) {
    const y = lumaAt(data, i);
    lumas[p] = y;
    sum += y;
  }

  const sorted = Uint8Array.from(lumas);
  sorted.sort();
  const pLow = sorted[Math.floor(pixels * 0.02)] ?? 0;
  const pHigh = sorted[Math.floor(pixels * 0.98)] ?? 255;
  const brightness = sum / pixels / 255;
  const spread = (pHigh - pLow) / 255;

  let varSum = 0;
  for (let p = 0; p < pixels; p++) {
    const d = lumas[p] / 255 - brightness;
    varSum += d * d;
  }
  const contrast = Math.sqrt(varSum / pixels);

  const darkThreshold = v.darkThreshold ?? 0.28;
  const brightThreshold = v.brightThreshold ?? 0.82;
  const lowContrast = v.lowContrast ?? 0.16;

  return {
    brightness,
    width,
    height,
    spread,
    contrast,
    pLow,
    pHigh,
    dark: brightness < darkThreshold,
    bright: brightness > brightThreshold,
    flat: spread < lowContrast,
    nearlyBlack: brightness < 0.06,
  };
}

function jpegQuality(buf) {
  try {
    const { data, width, height } = decodeJpeg(buf);
    return analyzeRgba(data, width, height);
  } catch {
    return analyzeRgba(null, 0, 0);
  }
}

function encodeJpeg(data, width, height, quality) {
  return Buffer.from(jpeg.encode({ data, width, height }, quality).data);
}

function stretchContrast(data, pLow, pHigh) {
  if (pHigh - pLow < 8) {
    return false;
  }
  const scale = 255 / (pHigh - pLow);
  for (let i = 0; i < data.length; i += 4) {
    const y = lumaAt(data, i);
    const y2 = clamp((y - pLow) * scale);
    const f = y > 0.5 ? y2 / y : 1;
    data[i] = clamp(data[i] * f);
    data[i + 1] = clamp(data[i + 1] * f);
    data[i + 2] = clamp(data[i + 2] * f);
  }
  return true;
}

function applyGamma(data, gamma) {
  if (gamma >= 0.995) {
    return false;
  }
  const inv = 1 / gamma;
  for (let i = 0; i < data.length; i += 4) {
    data[i] = clamp(255 * (data[i] / 255) ** inv);
    data[i + 1] = clamp(255 * (data[i + 1] / 255) ** inv);
    data[i + 2] = clamp(255 * (data[i + 2] / 255) ** inv);
  }
  return true;
}

function applyGain(data, gain) {
  if (Math.abs(gain - 1) < 0.03) {
    return false;
  }
  for (let i = 0; i < data.length; i += 4) {
    data[i] = clamp(data[i] * gain);
    data[i + 1] = clamp(data[i + 1] * gain);
    data[i + 2] = clamp(data[i + 2] * gain);
  }
  return true;
}

function applySharpen(data, width, height, amount) {
  if (amount <= 0 || width < 3 || height < 3) {
    return false;
  }
  const lum = new Float32Array(width * height);
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const i = (y * width + x) * 4;
      lum[y * width + x] = lumaAt(data, i);
    }
  }
  const out = Float32Array.from(lum);
  for (let y = 1; y < height - 1; y++) {
    for (let x = 1; x < width - 1; x++) {
      const c = lum[y * width + x];
      const blur =
        lum[(y - 1) * width + x] +
        lum[(y + 1) * width + x] +
        lum[y * width + x - 1] +
        lum[y * width + x + 1];
      out[y * width + x] = c + amount * (4 * c - blur);
    }
  }
  for (let y = 0; y < height; y++) {
    for (let x = 0; x < width; x++) {
      const p = y * width + x;
      const i = p * 4;
      const y0 = lumaAt(data, i);
      const delta = out[p] - y0;
      data[i] = clamp(data[i] + delta);
      data[i + 1] = clamp(data[i + 1] + delta);
      data[i + 2] = clamp(data[i + 2] + delta);
    }
  }
  return true;
}

function needsVisionWork(stats, v) {
  const target = v.targetBrightness ?? 0.44;
  return (
    stats.dark ||
    stats.bright ||
    stats.flat ||
    stats.brightness < target * 0.85 ||
    stats.brightness > (v.brightThreshold ?? 0.82)
  );
}

function prepareVisionJpeg(rawBuf) {
  const v = visionOpts();
  if (!CFG.perceptionAutoEnhance) {
    return { buf: rawBuf, enhanced: false, passes: [], quality: jpegQuality(rawBuf) };
  }

  let decoded;
  try {
    decoded = decodeJpeg(rawBuf);
  } catch {
    return { buf: rawBuf, enhanced: false, passes: [], quality: jpegQuality(rawBuf) };
  }

  const { data, width, height } = decoded;
  const passes = [];
  const maxPasses = Math.max(1, Math.min(8, v.maxPasses ?? 4));
  const target = v.targetBrightness ?? 0.44;
  const maxGain = v.maxGain ?? 3.2;
  const gammaMin = v.gammaMin ?? 0.5;
  const jpegQ = v.jpegQuality ?? 88;

  if (needsVisionWork(analyzeRgba(data, width, height, v), v)) {
    for (let pass = 0; pass < maxPasses; pass++) {
      const stats = analyzeRgba(data, width, height, v);
      let changed = false;

      if (v.contrastStretch !== false && stats.flat) {
        if (stretchContrast(data, stats.pLow, stats.pHigh)) {
          passes.push("contrast");
          changed = true;
        }
      }

      const mid = analyzeRgba(data, width, height, v);
      if (mid.dark || mid.brightness < target * 0.92) {
        const gamma = Math.min(
          1,
          Math.max(gammaMin, (mid.brightness / target) ** 0.85),
        );
        if (applyGamma(data, gamma)) {
          passes.push(`gamma:${gamma.toFixed(2)}`);
          changed = true;
        }
      }

      const afterGamma = analyzeRgba(data, width, height, v);
      if (afterGamma.dark || afterGamma.brightness < target * 0.9) {
        const gain = Math.min(maxGain, target / Math.max(afterGamma.brightness, 0.04));
        if (applyGain(data, gain)) {
          passes.push(`gain:${gain.toFixed(2)}`);
          changed = true;
        }
      } else if (afterGamma.bright) {
        const gain = Math.max(0.55, target / afterGamma.brightness);
        if (applyGain(data, gain)) {
          passes.push(`gain:${gain.toFixed(2)}`);
          changed = true;
        }
      }

      if (!changed) {
        break;
      }

      const done = analyzeRgba(data, width, height, v);
      if (
        done.brightness >= target * 0.85 &&
        done.brightness <= (v.brightThreshold ?? 0.82) &&
        done.spread >= (v.lowContrast ?? 0.16) * 0.75
      ) {
        break;
      }
    }
  }

  if ((v.sharpen ?? 0) > 0 && passes.length > 0) {
    if (applySharpen(data, width, height, v.sharpen)) {
      passes.push("sharpen");
    }
  }

  const quality = analyzeRgba(data, width, height, v);
  const buf = encodeJpeg(data, width, height, jpegQ);
  return {
    buf,
    enhanced: passes.length > 0,
    passes,
    quality: {
      ...quality,
      brightnessBefore: jpegQuality(rawBuf).brightness,
    },
  };
}

function enrichScene(scene, { motion, quality, vision } = {}) {
  const out = { ...scene };
  if (motion) {
    out.motion_score = motion.score;
    out.motion_level = motion.level ?? MotionLevel.NONE;
  }
  if (quality) {
    out.brightness = quality.brightness;
    out.lighting = quality.dark ? "dark" : quality.bright ? "bright" : "normal";
    if (quality.brightnessBefore != null) {
      out.brightness_before = quality.brightnessBefore;
    }
    if (quality.nearlyBlack) {
      out.lighting = "very_dark";
    }
  }
  if (vision?.enhanced) {
    out.enhanced = true;
    out.vision_passes = vision.passes;
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
  const names = tags
    .map((o) => (typeof o === "string" ? o : o?.name))
    .filter((n) => n && n !== "undefined");
  return names.length ? names.slice(0, 4).join(", ") : null;
}

function shouldAnalyze({ motion, frame, prev, forceIntervalMs, quality }) {
  const gapMs = msBetween(prev?.captured_at, frame.captured_at);
  const motionMin = quality?.dark
    ? (visionOpts().darkMotionMin ?? 0.08)
    : CFG.perceptionMotionMin;

  if (motion.level === MotionLevel.HIGH) {
    return true;
  }
  if (motion.level === MotionLevel.LOW && gapMs >= forceIntervalMs / 2) {
    return true;
  }
  if (gapMs >= forceIntervalMs) {
    return true;
  }
  return motion.changed && motion.score >= motionMin;
}

function markQuietFrame(db, frame, motion, quality, { reason = "static" } = {}) {
  const scene = enrichScene(
    {
      skipped: true,
      quiet: true,
      unchanged: true,
      summary: null,
      person_present: false,
      people: 0,
      scene: "static",
      activity: "quiet",
      objects: [],
      mood: "empty",
      skip_reason: reason,
    },
    { motion, quality },
  );
  markFrameProcessed(db, frame.id, null, JSON.stringify(scene));
  return { id: frame.id, skipped: reason, motion: motion.score, usedLm: false };
}

function sanitizeDarkScene(scene, quality, motion) {
  if (!quality?.dark && !quality?.nearlyBlack) {
    return scene;
  }
  const out = { ...scene };
  const summary = String(out.summary ?? "");
  const guessedPerson =
    out.person_present ||
    /pesso|algu[eé]m|deitad|sentad|smartphone|tablet|celular|cama|laptop|computador/i.test(summary);

  if (guessedPerson && (quality.nearlyBlack || quality.brightness < 0.2)) {
    out.person_present = false;
    out.people = 0;
    out.objects = [];
    out.summary =
      motion?.level === MotionLevel.HIGH
        ? "Movimento no escuro — detalhes incertos."
        : "Ambiente escuro — pouco visível.";
    out.activity = "unknown";
    out.hallucination_guard = true;
  }
  return out;
}

function markSkipped(db, frame, motion, quality) {
  return markQuietFrame(db, frame, motion, quality, { reason: "no_motion" });
}

export async function processFrame(db, frame, { allowLm = true } = {}) {
  const buf = readFileSync(frame.path);
  const quality = jpegQuality(buf);

  let motion = { score: 1, changed: true, level: MotionLevel.HIGH };
  const prev = lastProcessedFrame(db);
  if (prev?.path && existsSync(prev.path)) {
    motion = compareFrames(readFileSync(prev.path), buf, { dark: quality.dark });
  }

  if (frame.reason === "motion" && prev?.captured_at) {
    const gap = msBetween(prev.captured_at, frame.captured_at);
    if (gap < 45_000) {
      return markQuietFrame(db, frame, motion, quality, { reason: "motion_burst" });
    }
  }

  if (!shouldAnalyze({ motion, frame, prev, forceIntervalMs: CFG.perceptionMotionForceMs, quality })) {
    return markSkipped(db, frame, motion, quality);
  }

  let visionBuf = buf;
  let visionMeta = { enhanced: false, passes: [], quality };
  if (CFG.perceptionAutoEnhance) {
    const prepared = prepareVisionJpeg(buf);
    visionBuf = prepared.buf;
    visionMeta = prepared;
  }

  const lmQuality = visionMeta.quality ?? quality;
  const minBright = visionOpts().minBrightnessForLm ?? 0.1;
  if (lmQuality.nearlyBlack || lmQuality.brightness < minBright) {
    return markQuietFrame(db, frame, motion, lmQuality, { reason: "too_dark" });
  }

  let previousScene = null;
  if (prev?.scene_json) {
    try {
      previousScene = JSON.parse(prev.scene_json);
    } catch {
      /* ignore */
    }
  }

  if (
    previousScene &&
    !previousScene.skipped &&
    motion.level === MotionLevel.LOW &&
    msBetween(prev?.captured_at, frame.captured_at) < CFG.perceptionMotionForceMs / 3
  ) {
    return markQuietFrame(db, frame, motion, lmQuality, { reason: "same_scene" });
  }

  if (!allowLm) {
    return { id: frame.id, deferred: true, usedLm: false };
  }

  const sceneRaw = await captionFrame(visionBuf.toString("base64"), frame.reason, {
    previousCaption: prev?.caption,
    previousScene,
    quality: lmQuality,
    motion,
    vision: visionMeta,
  });

  const guarded = sanitizeDarkScene(sceneRaw, lmQuality, motion);
  const scene = enrichScene(guarded, { motion, quality: lmQuality, vision: visionMeta });
  if (scene.unchanged) {
    return markQuietFrame(db, frame, motion, lmQuality, { reason: "unchanged" });
  }
  if (scene.hallucination_guard && motion.level !== MotionLevel.HIGH) {
    return markQuietFrame(db, frame, motion, lmQuality, { reason: "dark_uncertain" });
  }

  const caption = formatSceneCaption(scene);
  if (!caption?.trim()) {
    return markQuietFrame(db, frame, motion, lmQuality, { reason: "empty_caption" });
  }
  if (isDarkSceneCaption(caption)) {
    return markQuietFrame(db, frame, motion, lmQuality, { reason: "dark_caption" });
  }
  const tags = objectsSummary(scene);
  const finalCaption = tags && !caption.includes(tags) ? `${caption} (${tags})` : caption;

  markFrameProcessed(db, frame.id, finalCaption, JSON.stringify(scene));

  const people = Number(scene.people) || 0;
  const personPresent = scene.person_present === true;

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
