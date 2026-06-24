import { execFile } from "node:child_process";
import { promisify } from "node:util";
import { CFG } from "./config.mjs";

const execFileAsync = promisify(execFile);

const state = {
  ready: false,
  backend: null,
  model: null,
  bin: null,
  device: null,
  checkedAt: 0,
};

/** openai-whisper on the host — not LM Studio. */
export async function refreshSttCapability({ force = false } = {}) {
  if (!force && state.checkedAt && Date.now() - state.checkedAt < 60_000) {
    return { ...state };
  }

  const bin = CFG.whisperBin;
  try {
    await execFileAsync(bin, ["--help"], { timeout: 6000, env: process.env });
    state.ready = true;
    state.backend = "cli";
    state.bin = bin;
    state.model = CFG.whisperModel || process.env.FOLIO_WHISPER_MODEL || "base";
    state.device = CFG.whisperDevice;
  } catch {
    state.ready = false;
    state.backend = null;
    state.bin = bin;
    state.model = null;
    state.device = null;
  }

  state.checkedAt = Date.now();
  return { ...state };
}

export function sttCapability() {
  return { ...state };
}

export function sttActive() {
  if (CFG.audioSttEnabled === false) {
    return false;
  }
  return state.ready;
}
