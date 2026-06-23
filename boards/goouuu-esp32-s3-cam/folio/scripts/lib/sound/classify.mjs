import { CFG } from "../config/index.mjs";
import { pcmFingerprint } from "../speaker/fingerprint.mjs";

const SOUND_LABELS = {
  speech: "fala",
  bark: "latido",
  door: "porta/impacto",
  ambient: "ambiente",
  unknown: "som",
};

export function classifySoundChunk(pcmBuffer, energy, durationMs = CFG.audioChunkMs) {
  const fp = pcmFingerprint(pcmBuffer);
  const zcr = fp[1] ?? 0;
  const rms = fp[0] ?? energy ?? 0;

  if (energy >= CFG.speechEnergyThreshold) {
    return { kind: "speech", label: SOUND_LABELS.speech, confidence: 0.9 };
  }

  if (energy >= 0.003 && energy < 0.045 && zcr > 0.07 && durationMs <= 2500) {
    return { kind: "bark", label: SOUND_LABELS.bark, confidence: 0.72 };
  }

  if (energy >= 0.012 && durationMs <= 1200 && rms > 0.02) {
    return { kind: "door", label: SOUND_LABELS.door, confidence: 0.68 };
  }

  if (energy >= CFG.ambientEnergyThreshold) {
    return { kind: "ambient", label: SOUND_LABELS.ambient, confidence: 0.55 };
  }

  return { kind: "unknown", label: SOUND_LABELS.unknown, confidence: 0.3 };
}
