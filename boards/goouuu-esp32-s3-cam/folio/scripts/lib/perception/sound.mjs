import { CFG } from "../config.mjs";
import { pcmFingerprint } from "../speaker.mjs";

export const SoundKind = Object.freeze({
  SPEECH: "speech",
  BARK: "bark",
  DOOR: "door",
  KNOCK: "knock",
  APPLIANCE: "appliance",
  UNKNOWN: "unknown",
});

/** Classifier shape — not user config; ranked against calibrated energy gates. */
const HEURISTIC = Object.freeze({
  speech: { confidence: 0.92 },
  bark: { confidence: 0.74, maxEnergy: 0.045, minZcr: 0.07, maxDurationMs: 2500 },
  door: { confidence: 0.7, minEnergy: 0.01, maxDurationMs: 900, minRms: 0.018, maxZcr: 0.05 },
  knock: { confidence: 0.62, minEnergy: 0.008, maxDurationMs: 600, minZcr: 0.12 },
  appliance: { confidence: 0.55, maxZcr: 0.04, minDurationMs: 800 },
  unknown: { confidence: 0.4, lowConfidence: 0.15 },
});

function soundLabel(kind) {
  return CFG.perceptionSoundLabels?.[kind] ?? kind;
}

function hit(kind, confidence) {
  return { kind, label: soundLabel(kind), confidence };
}

function classifySoundHeuristic(pcmBuffer, energy, durationMs) {
  const fp = pcmFingerprint(pcmBuffer);
  const zcr = fp[1] ?? 0;
  const rms = fp[0] ?? energy ?? 0;
  const dur = durationMs ?? CFG.audioChunkMs;
  const speechGate = CFG.speechEnergyThreshold;
  const soundGate = CFG.perceptionSoundMinEnergy;

  if (energy >= speechGate) {
    return hit(SoundKind.SPEECH, HEURISTIC.speech.confidence);
  }

  const bark = HEURISTIC.bark;
  if (
    energy >= soundGate &&
    energy < bark.maxEnergy &&
    zcr > bark.minZcr &&
    dur <= bark.maxDurationMs
  ) {
    return hit(SoundKind.BARK, bark.confidence);
  }

  const door = HEURISTIC.door;
  if (
    energy >= door.minEnergy &&
    dur <= door.maxDurationMs &&
    rms > door.minRms &&
    zcr < door.maxZcr
  ) {
    return hit(SoundKind.DOOR, door.confidence);
  }

  const knock = HEURISTIC.knock;
  if (energy >= knock.minEnergy && dur <= knock.maxDurationMs && zcr > knock.minZcr) {
    return hit(SoundKind.KNOCK, knock.confidence);
  }

  const appliance = HEURISTIC.appliance;
  if (energy >= soundGate && zcr < appliance.maxZcr && dur > appliance.minDurationMs) {
    return hit(SoundKind.APPLIANCE, appliance.confidence);
  }

  if (energy >= soundGate) {
    return hit(SoundKind.UNKNOWN, HEURISTIC.unknown.confidence);
  }

  return hit(SoundKind.UNKNOWN, HEURISTIC.unknown.lowConfidence);
}

export async function classifySound(pcmBuffer, energy, durationMs) {
  return classifySoundHeuristic(pcmBuffer, energy, durationMs);
}

export function isInterestingSound(result) {
  return result.kind !== SoundKind.UNKNOWN && result.kind !== SoundKind.SPEECH;
}

export function speechLabel() {
  return soundLabel(SoundKind.SPEECH);
}
