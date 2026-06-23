import { execFile } from "node:child_process";
import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { basename, join } from "node:path";
import { promisify } from "node:util";
import { whisperLanguageCode } from "../locale/index.mjs";
import { modelId, ModelSlot, whisperRuntime } from "../models/index.mjs";

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
    return { text, segments, confidence };
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
