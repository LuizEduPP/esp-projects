import { CFG } from "./config.mjs";
import { transcribeAudio } from "./llm.mjs";
import { whisperLanguageCode } from "./locale.mjs";
import { modelId, ModelSlot } from "./models.mjs";

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

export function shouldRejectTranscript(stt) {
  const text = String(stt?.text ?? "").trim();
  if (!text) {
    return false;
  }
  const lower = text.toLowerCase();
  for (const pat of CFG.audioSttRejectPatterns ?? []) {
    if (pat && lower.includes(String(pat).toLowerCase())) {
      return true;
    }
  }
  return false;
}

/** LM Studio STT — optional; most setups use one chat/vision model only (sttEnabled=false). */
export async function transcribeWav(wavPath, { chunkId } = {}) {
  if (!CFG.audioSttEnabled) {
    return { text: "", segments: [], confidence: 0, skipped: "stt_disabled" };
  }

  const model = modelId(ModelSlot.FAST);
  const tag = chunkId != null ? `chunk=${chunkId}` : wavPath;
  const t0 = Date.now();
  const lang = CFG.sttLanguage ? whisperLanguageCode() : null;

  console.log(`[stt] ${tag} start model=${model} lang=${lang ?? "auto"}`);

  try {
    const raw = await transcribeAudio(wavPath, {
      model,
      language: lang,
      timeoutMs: CFG.sttTimeoutMs,
    });

    const text = String(raw.text ?? "").trim();
    const ms = Date.now() - t0;
    const confidence = text ? 0.85 : 0;

    if (text) {
      const preview = text.length > 160 ? `${text.slice(0, 160)}…` : text;
      console.log(`[stt] ${tag} ok ${ms}ms "${preview}"`);
    } else {
      console.log(`[stt] ${tag} empty ${ms}ms`);
    }

    const result = { text, segments: [], confidence };
    if (shouldRejectTranscript(result)) {
      return { text: "", segments: [], confidence: 0 };
    }
    return result;
  } catch (err) {
    console.error(`[stt] ${tag} fail ${Date.now() - t0}ms — ${err.message}`);
    throw new Error(`LM Studio STT failed: ${err.message}`);
  }
}
