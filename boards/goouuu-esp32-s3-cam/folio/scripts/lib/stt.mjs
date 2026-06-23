import { execFile } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { promisify } from "node:util";
import { CFG } from "./config/index.mjs";
import { whisperLanguageCode } from "./locale/index.mjs";
import { modelId, ModelSlot, whisperRuntime } from "./models/index.mjs";

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

function findWhisperJson(outDir, wavPath) {
  const stem = basename(wavPath).replace(/\.wav$/i, "");
  const direct = join(outDir, `${stem}.json`);
  if (existsSync(direct)) {
    return direct;
  }
  for (const name of readdirSync(outDir)) {
    if (name.endsWith(".json")) {
      return join(outDir, name);
    }
  }
  return null;
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
  const probs = (stt?.segments ?? [])
    .map((s) => s.noSpeechProb)
    .filter((p) => typeof p === "number");
  if (probs.length > 0) {
    const avg = probs.reduce((a, b) => a + b, 0) / probs.length;
    if (avg >= CFG.audioSttMaxNoSpeechProb) {
      return true;
    }
  }
  return false;
}

export async function transcribeWav(wavPath, { chunkId } = {}) {
  const { bin, device, timeoutMs, language } = whisperRuntime();
  const model = modelId(ModelSlot.WHISPER);
  const tag = chunkId != null ? `chunk=${chunkId}` : basename(wavPath);
  const outDir = join(tmpdir(), `folio-whisper-${Date.now()}-${process.pid}`);
  mkdirSync(outDir, { recursive: true });
  const t0 = Date.now();
  const lang = language ? whisperLanguageCode() : null;

  console.log(
    `[whisper] ${tag} start model=${model} lang=${lang ?? "auto"} ` +
      `device=${device} bin=${bin}`,
  );

  const args = [
    wavPath,
    "--model",
    model,
    "--output_format",
    "json",
    "--output_dir",
    outDir,
    "--device",
    device,
  ];
  if (lang) {
    args.push("--language", lang);
  }

  try {
    await execFileAsync(bin, args, {
      timeout: timeoutMs,
      env: process.env,
    });

    const outJson = findWhisperJson(outDir, wavPath);
    if (!outJson) {
      throw new Error(`Whisper produced no JSON in ${outDir}`);
    }

    const raw = JSON.parse(readFileSync(outJson, "utf8"));
    const text = String(raw.text ?? "").trim();
    const segments = (raw.segments ?? []).map((s) => ({
      start: s.start,
      end: s.end,
      text: String(s.text ?? "").trim(),
      noSpeechProb: s.no_speech_prob,
    }));
    const ms = Date.now() - t0;
    const probs = segments.map((s) => s.noSpeechProb).filter((p) => typeof p === "number");
    const confidence =
      probs.length > 0
        ? Math.max(0, Math.min(1, 1 - probs.reduce((a, b) => a + b, 0) / probs.length))
        : text
          ? 0.85
          : 0;
    if (text) {
      const preview = text.length > 160 ? `${text.slice(0, 160)}…` : text;
      console.log(`[whisper] ${tag} ok ${ms}ms (${segments.length} seg) "${preview}"`);
    } else {
      console.log(`[whisper] ${tag} empty ${ms}ms — no speech in audio`);
    }
    const result = { text, segments, confidence };
    if (shouldRejectTranscript(result)) {
      console.log(`[whisper] ${tag} rejected — hallucination / no-speech (${text.slice(0, 80)})`);
      return { text: "", segments, confidence: 0 };
    }
    return result;
  } catch (err) {
    const ms = Date.now() - t0;
    if (err.code === "ENOENT") {
      console.error(`[whisper] ${tag} fail ${ms}ms — CLI not found (${bin})`);
      throw new Error(
        `Whisper CLI not found (${bin}). Install openai-whisper or set FOLIO_WHISPER_BIN.`,
      );
    }
    const detail = err.stderr?.toString?.() || err.stdout?.toString?.() || err.message;
    console.error(`[whisper] ${tag} fail ${ms}ms — ${String(detail).split("\n").slice(-2).join(" ").trim()}`);
    throw new Error(`Whisper failed: ${String(detail).split("\n").slice(-3).join(" ").trim()}`);
  } finally {
    try {
      rmSync(outDir, { recursive: true, force: true });
    } catch {
      /* ignore */
    }
  }
}
