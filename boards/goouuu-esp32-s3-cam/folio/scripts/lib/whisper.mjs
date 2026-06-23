import { execFile } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { promisify } from "node:util";
import { CFG } from "./config.mjs";
import { whisperLanguageCode } from "./locale.mjs";

const execFileAsync = promisify(execFile);

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

export async function transcribeWav(wavPath) {
  const outDir = join(tmpdir(), `folio-whisper-${Date.now()}-${process.pid}`);
  mkdirSync(outDir, { recursive: true });

  try {
    await execFileAsync(
      CFG.whisperBin,
      [
        wavPath,
        "--model",
        CFG.whisperModel,
        "--language",
        whisperLanguageCode(),
        "--output_format",
        "json",
        "--output_dir",
        outDir,
        "--device",
        CFG.whisperDevice,
      ],
      {
        timeout: CFG.whisperTimeoutMs,
        env: process.env,
      },
    );

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
    }));
    return { text, segments, confidence: segments.length ? 0.85 : 0 };
  } catch (err) {
    if (err.code === "ENOENT") {
      throw new Error(
        `Whisper CLI not found (${CFG.whisperBin}). Install openai-whisper or set FOLIO_WHISPER_BIN.`,
      );
    }
    const detail = err.stderr?.toString?.() || err.stdout?.toString?.() || err.message;
    throw new Error(`Whisper failed: ${String(detail).split("\n").slice(-3).join(" ").trim()}`);
  } finally {
    try {
      rmSync(outDir, { recursive: true, force: true });
    } catch {
      /* ignore */
    }
  }
}

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

/** True when buffer is missing, zero-length, or all-zero samples. */
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

/** Ingest gate: only persist chunks with real audio energy. */
export function shouldStoreAudioChunk(pcmBuffer) {
  if (pcmIsEmpty(pcmBuffer)) {
    return { store: false, energy: 0, reason: "empty" };
  }
  const energy = pcmEnergy(pcmBuffer);
  if (!isSpeechChunk(energy)) {
    return { store: false, energy, reason: "quiet" };
  }
  return { store: true, energy, reason: null };
}
