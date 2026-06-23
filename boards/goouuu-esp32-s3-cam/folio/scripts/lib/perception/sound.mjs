import { CFG } from "../config/index.mjs";
import { pcmFingerprint } from "../speaker.mjs";
import { classifySoundYamnet } from "./yamnet.mjs";
import { SoundKind } from "./types.mjs";

function soundLabel(kind) {
  return CFG.perceptionSoundLabels?.[kind] ?? kind;
}

function hit(kind, confidence) {
  return { kind, label: soundLabel(kind), confidence };
}

function ruleNum(rule, key, fallback = null) {
  const v = rule?.[key];
  return v == null ? fallback : Number(v);
}

function classifySoundHeuristic(pcmBuffer, energy, durationMs) {
  const rules = CFG.perceptionHeuristic ?? {};
  const fp = pcmFingerprint(pcmBuffer);
  const zcr = fp[1] ?? 0;
  const rms = fp[0] ?? energy ?? 0;
  const dur = durationMs ?? CFG.audioChunkMs;

  if (energy >= CFG.speechEnergyThreshold) {
    return hit(SoundKind.SPEECH, ruleNum(rules.speech, "confidence", 0.92));
  }

  const bark = rules.bark ?? {};
  if (
    energy >= CFG.perceptionSoundMinEnergy &&
    energy < ruleNum(bark, "maxEnergy", 0.045) &&
    zcr > ruleNum(bark, "minZcr", 0.07) &&
    dur <= ruleNum(bark, "maxDurationMs", 2500)
  ) {
    return hit(SoundKind.BARK, ruleNum(bark, "confidence", 0.74));
  }

  const door = rules.door ?? {};
  if (
    energy >= ruleNum(door, "minEnergy", 0.01) &&
    dur <= ruleNum(door, "maxDurationMs", 900) &&
    rms > ruleNum(door, "minRms", 0.018) &&
    zcr < ruleNum(door, "maxZcr", 0.05)
  ) {
    return hit(SoundKind.DOOR, ruleNum(door, "confidence", 0.7));
  }

  const knock = rules.knock ?? {};
  if (
    energy >= ruleNum(knock, "minEnergy", 0.008) &&
    dur <= ruleNum(knock, "maxDurationMs", 600) &&
    zcr > ruleNum(knock, "minZcr", 0.12)
  ) {
    return hit(SoundKind.KNOCK, ruleNum(knock, "confidence", 0.62));
  }

  const appliance = rules.appliance ?? {};
  if (
    energy >= CFG.perceptionSoundMinEnergy &&
    zcr < ruleNum(appliance, "maxZcr", 0.04) &&
    dur > ruleNum(appliance, "minDurationMs", 800)
  ) {
    return hit(SoundKind.APPLIANCE, ruleNum(appliance, "confidence", 0.55));
  }

  const unknown = rules.unknown ?? {};
  if (energy >= CFG.perceptionSoundMinEnergy) {
    return hit(SoundKind.UNKNOWN, ruleNum(unknown, "confidence", 0.4));
  }

  return hit(SoundKind.UNKNOWN, ruleNum(unknown, "lowConfidence", 0.15));
}

export async function classifySound(pcmBuffer, energy, durationMs) {
  if (CFG.perceptionSoundEngine === "yamnet") {
    return classifySoundYamnet(pcmBuffer, energy);
  }
  return classifySoundHeuristic(pcmBuffer, energy, durationMs);
}

export function isInterestingSound(result) {
  return (
    result.kind !== SoundKind.UNKNOWN &&
    result.kind !== SoundKind.SPEECH &&
    result.confidence >= CFG.perceptionSoundMinConfidence
  );
}

export function speechLabel() {
  return soundLabel(SoundKind.SPEECH);
}
