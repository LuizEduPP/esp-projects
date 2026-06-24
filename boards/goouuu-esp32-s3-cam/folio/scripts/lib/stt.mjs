import { readFileSync, unlinkSync } from "node:fs";
import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { CFG } from "./config.mjs";
import { transcribeAudio } from "./llm.mjs";
import { whisperLanguageCode } from "./locale.mjs";
import { refreshSttCapability, sttActive, sttCapability } from "./stt-capability.mjs";
import { isSttHallucination } from "./util.mjs";

const execFileAsync = promisify(execFile);

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
  if (isSttHallucination(text, CFG.audioSttRejectPatterns)) {
    return true;
  }
  const lower = text.toLowerCase();
  for (const pat of CFG.audioSttRejectPatterns ?? []) {
    if (pat && lower.includes(String(pat).toLowerCase())) {
      return true;
    }
  }
  return false;
}

const WHISPER_LANG_NAME = {
  pt: "Portuguese",
  en: "English",
  es: "Spanish",
  fr: "French",
  de: "German",
  it: "Italian",
  ja: "Japanese",
  zh: "Chinese",
};

async function transcribeWhisperCli(wavPath, { chunkId } = {}) {
  const cap = sttCapability();
  const tag = chunkId != null ? `chunk=${chunkId}` : wavPath;
  const lang = whisperLanguageCode();
  const args = [
    wavPath,
    "--model",
    cap.model || "base",
    "--output_format",
    "json",
    "--fp16",
    "False",
    "--device",
    cap.device || "cpu",
  ];
  if (lang) {
    args.push("--language", WHISPER_LANG_NAME[lang] ?? lang);
  }

  await execFileAsync(cap.bin, args, {
    timeout: CFG.sttTimeoutMs,
    env: process.env,
  });

  const jsonPath = wavPath.replace(/\.wav$/i, ".json");
  const raw = JSON.parse(readFileSync(jsonPath, "utf8"));
  try {
    unlinkSync(jsonPath);
  } catch {
    /* ignore */
  }

  const text = String(raw.text ?? "").trim();
  console.log(`[whisper] ${tag} cli ok "${text.slice(0, 120)}${text.length > 120 ? "…" : ""}"`);
  return { text, segments: raw.segments ?? [], confidence: text ? 0.9 : 0 };
}

async function transcribeWhisperLm(wavPath, { chunkId } = {}) {
  const cap = sttCapability();
  const tag = chunkId != null ? `chunk=${chunkId}` : wavPath;
  const lang = CFG.sttLanguage ? whisperLanguageCode() : null;
  const raw = await transcribeAudio(wavPath, {
    model: cap.model,
    language: lang,
    timeoutMs: CFG.sttTimeoutMs,
  });
  const text = String(raw.text ?? "").trim();
  console.log(`[whisper] ${tag} lm ok "${text.slice(0, 120)}${text.length > 120 ? "…" : ""}"`);
  return { text, segments: [], confidence: text ? 0.85 : 0 };
}

/** Whisper CLI or LM Studio /v1/audio/transcriptions — auto when available. */
export async function transcribeWav(wavPath, { chunkId } = {}) {
  await refreshSttCapability();
  if (!sttActive()) {
    return { text: "", segments: [], confidence: 0, skipped: "no_whisper" };
  }

  const cap = sttCapability();
  const tag = chunkId != null ? `chunk=${chunkId}` : wavPath;
  const t0 = Date.now();

  try {
    const result =
      cap.backend === "cli"
        ? await transcribeWhisperCli(wavPath, { chunkId })
        : await transcribeWhisperLm(wavPath, { chunkId });

    if (shouldRejectTranscript(result)) {
      console.log(`[whisper] ${tag} rejected hallucination ${Date.now() - t0}ms`);
      return { text: "", segments: [], confidence: 0 };
    }
    return result;
  } catch (err) {
    console.error(`[whisper] ${tag} fail ${Date.now() - t0}ms — ${err.message}`);
    await refreshSttCapability({ force: true });
    throw err;
  }
}

export { refreshSttCapability, sttActive, sttCapability };
