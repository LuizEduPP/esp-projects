import { CFG } from "../config/index.mjs";
import { pcmFingerprint } from "../speaker/fingerprint.mjs";
import { SoundKind } from "./types.mjs";

const LABELS = {
  [SoundKind.SPEECH]: "fala",
  [SoundKind.BARK]: "latido",
  [SoundKind.DOOR]: "porta",
  [SoundKind.KNOCK]: "batida",
  [SoundKind.APPLIANCE]: "eletrodoméstico",
  [SoundKind.UNKNOWN]: "som",
};

export function classifySound(pcmBuffer, energy, durationMs) {
  const fp = pcmFingerprint(pcmBuffer);
  const zcr = fp[1] ?? 0;
  const rms = fp[0] ?? energy ?? 0;
  const dur = durationMs ?? CFG.audioChunkMs ?? 1000;

  if (energy >= CFG.speechEnergyThreshold) {
    return hit(SoundKind.SPEECH, 0.92);
  }

  if (energy >= CFG.perceptionSoundMinEnergy && energy < 0.045 && zcr > 0.07 && dur <= 2500) {
    return hit(SoundKind.BARK, 0.74);
  }

  if (energy >= 0.01 && dur <= 900 && rms > 0.018 && zcr < 0.05) {
    return hit(SoundKind.DOOR, 0.7);
  }

  if (energy >= 0.008 && dur <= 600 && zcr > 0.12) {
    return hit(SoundKind.KNOCK, 0.62);
  }

  if (energy >= CFG.perceptionSoundMinEnergy && zcr < 0.04 && dur > 800) {
    return hit(SoundKind.APPLIANCE, 0.55);
  }

  if (energy >= CFG.perceptionSoundMinEnergy) {
    return hit(SoundKind.UNKNOWN, 0.4);
  }

  return hit(SoundKind.UNKNOWN, 0.15);
}

function hit(kind, confidence) {
  return { kind, label: LABELS[kind] ?? kind, confidence };
}

export function isInterestingSound(result) {
  return (
    result.kind !== SoundKind.UNKNOWN &&
    result.kind !== SoundKind.SPEECH &&
    result.confidence >= CFG.perceptionSoundMinConfidence
  );
}
