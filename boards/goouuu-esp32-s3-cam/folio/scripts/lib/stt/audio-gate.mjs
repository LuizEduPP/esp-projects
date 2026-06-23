import { CFG } from "../config/index.mjs";

function pcmSamples(pcmBuffer) {
  if (!pcmBuffer?.length) {
    return new Int16Array(0);
  }
  return new Int16Array(pcmBuffer.buffer, pcmBuffer.byteOffset, pcmBuffer.length / 2);
}

export function pcmEnergy(pcmBuffer) {
  const samples = pcmSamples(pcmBuffer);
  if (samples.length === 0) {
    return 0;
  }
  let sum = 0;
  for (let i = 0; i < samples.length; i++) {
    const n = samples[i] / 32768;
    sum += n * n;
  }
  return Math.sqrt(sum / samples.length);
}

export function pcmIsEmpty(pcmBuffer) {
  const samples = pcmSamples(pcmBuffer);
  for (let i = 0; i < samples.length; i++) {
    if (samples[i] !== 0) {
      return false;
    }
  }
  return true;
}

export function isSpeechChunk(energy, threshold = CFG.speechEnergyThreshold) {
  return energy >= threshold;
}

/** Store only voiced segments — no ambient/noise archive. */
export function shouldStoreAudioChunk(pcmBuffer, energyOverride = null) {
  if (pcmIsEmpty(pcmBuffer)) {
    return { store: false, energy: 0, reason: "empty" };
  }
  const energy = energyOverride ?? pcmEnergy(pcmBuffer);
  if (!isSpeechChunk(energy)) {
    return { store: false, energy, reason: "quiet" };
  }
  return { store: true, energy, reason: null };
}
