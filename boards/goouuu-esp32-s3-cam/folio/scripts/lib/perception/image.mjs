import jpeg from "jpeg-js";
import { MotionLevel } from "./types.mjs";

const GREY_SAMPLES = 32;

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

export function jpegQuality(buf) {
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

export function compareFrames(prevBuf, nextBuf) {
  const score = motionScore(prevBuf, nextBuf);
  return {
    score,
    changed: score >= 0.035,
    level: score >= 0.12 ? MotionLevel.HIGH : score >= 0.035 ? MotionLevel.LOW : MotionLevel.NONE,
  };
}

export function enhanceJpeg(buf, { target = 0.42, maxGain = 2.2 } = {}) {
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

function clamp(v) {
  return Math.max(0, Math.min(255, Math.round(v)));
}
