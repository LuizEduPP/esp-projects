import { execFile } from "node:child_process";
import { readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { promisify } from "node:util";
import { CFG } from "./config.mjs";

const execFileAsync = promisify(execFile);

export async function transcribeWav(wavPath) {
  const outBase = join(tmpdir(), `folio-whisper-${Date.now()}`);
  const outJson = `${outBase}.json`;

  try {
    await execFileAsync(
      CFG.whisperBin,
      [
        wavPath,
        "--model",
        CFG.whisperModel,
        "--language",
        "Portuguese",
        "--output_format",
        "json",
        "--output_file",
        outBase,
        "--fp16",
        "False",
      ],
      { timeout: 120000 },
    );
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
    throw err;
  } finally {
    try {
      unlinkSync(outJson);
    } catch {
      /* ignore */
    }
  }
}

export function pcmEnergy(pcmBuffer) {
  const samples = new Int16Array(
    pcmBuffer.buffer,
    pcmBuffer.byteOffset,
    pcmBuffer.length / 2,
  );
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

export function isSpeechChunk(energy, threshold = 0.008) {
  return energy >= threshold;
}

export async function transcribePcmToWav(wavPath) {
  return transcribeWav(wavPath);
}

export function writeTempWav(pcmBuffer) {
  const path = join(tmpdir(), `folio-chunk-${Date.now()}.wav`);
  const header = Buffer.alloc(44);
  const dataSize = pcmBuffer.length;
  header.write("RIFF", 0);
  header.writeUInt32LE(36 + dataSize, 4);
  header.write("WAVE", 8);
  header.write("fmt ", 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(1, 22);
  header.writeUInt32LE(16000, 24);
  header.writeUInt32LE(32000, 28);
  header.writeUInt16LE(2, 32);
  header.writeUInt16LE(16, 34);
  header.write("data", 36);
  header.writeUInt32LE(dataSize, 40);
  writeFileSync(path, Buffer.concat([header, pcmBuffer]));
  return path;
}
