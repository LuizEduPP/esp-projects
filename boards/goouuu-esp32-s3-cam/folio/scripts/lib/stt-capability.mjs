import { execFile, execSync } from "node:child_process";
import { promisify } from "node:util";
import { CFG, isCudaAvailable } from "./config.mjs";
import { fetchOpenAiModels } from "./llm.mjs";

const execFileAsync = promisify(execFile);

const state = {
  ready: false,
  backend: null,
  model: null,
  bin: null,
  device: null,
  checkedAt: 0,
};

function resolveWhisperBin() {
  const env = process.env.FOLIO_WHISPER_BIN;
  if (env) {
    return env;
  }
  const file = CFG.whisperBin;
  if (file && file !== "whisper") {
    return file;
  }
  try {
    return execSync("which whisper", { encoding: "utf8", env: process.env }).trim() || "whisper";
  } catch {
    return file || "whisper";
  }
}

function resolveWhisperDevice() {
  const raw = process.env.FOLIO_WHISPER_DEVICE || CFG.whisperDevice || "auto";
  if (raw === "auto") {
    return isCudaAvailable() ? "cuda" : "cpu";
  }
  if (raw === "cuda" && !isCudaAvailable()) {
    return "cpu";
  }
  return raw;
}

function pickWhisperModel(ids = []) {
  const preferred = [
    CFG.lmModelWhisper,
    CFG.whisperModel,
    process.env.FOLIO_WHISPER_MODEL,
  ].filter(Boolean);
  for (const p of preferred) {
    if (ids.includes(p)) {
      return p;
    }
  }
  return ids.find((id) => /whisper|distil-whisper|faster-whisper/i.test(id)) ?? null;
}

async function probeCli() {
  const bin = resolveWhisperBin();
  try {
    await execFileAsync(bin, ["--help"], { timeout: 6000, env: process.env });
    const model =
      CFG.whisperModel ||
      process.env.FOLIO_WHISPER_MODEL ||
      CFG.lmModelWhisper ||
      "base";
    return {
      ready: true,
      backend: "cli",
      bin,
      model,
      device: resolveWhisperDevice(),
    };
  } catch {
    return null;
  }
}

async function probeLm() {
  const catalog = await fetchOpenAiModels();
  if (!catalog.ok) {
    return null;
  }
  const whisperIds = (catalog.all ?? []).filter((id) =>
    /whisper|distil-whisper|faster-whisper/i.test(String(id)),
  );
  const model = pickWhisperModel(whisperIds);
  if (!model) {
    return null;
  }
  return { ready: true, backend: "lm", model, bin: null, device: null };
}

/** Probe Whisper CLI or LM Studio /v1/audio/transcriptions model. Cached ~60s. */
export async function refreshSttCapability({ force = false } = {}) {
  if (!force && state.checkedAt && Date.now() - state.checkedAt < 60_000) {
    return { ...state };
  }

  const cli = await probeCli();
  const lm = cli ? null : await probeLm();
  const found = cli ?? lm;

  state.ready = !!found?.ready;
  state.backend = found?.backend ?? null;
  state.model = found?.model ?? null;
  state.bin = found?.bin ?? null;
  state.device = found?.device ?? null;
  state.checkedAt = Date.now();

  return { ...state };
}

export function sttCapability() {
  return { ...state };
}

/** STT runs when Whisper is available unless explicitly disabled in config. */
export function sttActive() {
  if (CFG.audioSttEnabled === false) {
    return false;
  }
  return state.ready;
}
