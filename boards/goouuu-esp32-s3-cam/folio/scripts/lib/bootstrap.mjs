/**
 * Runtime bootstrap — auto-detect LM Studio, models, STT, embeddings.
 */
import { networkInterfaces } from "node:os";
import { applyCalibrationToCfg } from "./calibration.mjs";
import { CFG, reloadConfig, userConfigOverrides } from "./config.mjs";
import { openDb } from "./db.mjs";
import { fetchOpenAiModels, joinOpenAiPath, normalizeOpenAiBase } from "./llm.mjs";
import { refreshSttCapability } from "./stt-capability.mjs";

const runtime = {
  bootstrapped: false,
  models: null,
  stt: null,
  embeddings: false,
  notes: [],
};

function listLanIpv4() {
  const ips = [];
  for (const ifaces of Object.values(networkInterfaces())) {
    for (const iface of ifaces ?? []) {
      if (iface.family === "IPv4" && !iface.internal) {
        ips.push(iface.address);
      }
    }
  }
  return ips;
}

async function probeLmUrl(url) {
  try {
    const base = normalizeOpenAiBase(url);
    const res = await fetch(joinOpenAiPath(base, "models"), {
      signal: AbortSignal.timeout(2500),
    });
    if (res.ok) {
      return base;
    }
  } catch {
    /* try next */
  }
  return null;
}

/** Find LM Studio on localhost, LAN, or env — no manual URL in config. */
async function discoverLmStudio(hintUrl) {
  const seen = new Set();
  const candidates = [];
  for (const u of [
    hintUrl,
    process.env.LM_URL,
    process.env.FOLIO_LM_URL,
    "http://127.0.0.1:1234/v1",
    "http://localhost:1234/v1",
  ]) {
    if (u && !seen.has(u)) {
      seen.add(u);
      candidates.push(u);
    }
  }
  for (const ip of listLanIpv4()) {
    const u = `http://${ip}:1234/v1`;
    if (!seen.has(u)) {
      seen.add(u);
      candidates.push(u);
    }
  }
  for (const url of candidates) {
    const base = await probeLmUrl(url);
    if (base) {
      return { ok: true, baseUrl: base };
    }
  }
  return { ok: false };
}

function requireChatModel(catalog, configured) {
  if (!catalog.ok || !catalog.chat?.length) {
    throw new Error(catalog.error ?? "LM Studio unreachable or no chat model loaded");
  }
  if (configured) {
    if (!catalog.chat.includes(configured)) {
      throw new Error(`Model not loaded in LM Studio: ${configured}`);
    }
    return configured;
  }
  if (catalog.chat.length === 1) {
    return catalog.chat[0];
  }
  const vision = catalog.chat.find((id) =>
    /vision|vl-|llava|ministral|pixtral|gemma.*vision/i.test(id),
  );
  if (vision) {
    return vision;
  }
  throw new Error(
    `Multiple chat models — pick one in Settings: ${catalog.chat.join(", ")}`,
  );
}

function requireEmbedModel(catalog, configured) {
  if (!catalog.ok || !catalog.embed?.length) {
    return null;
  }
  if (configured && catalog.embed.includes(configured)) {
    return configured;
  }
  if (catalog.embed.length === 1) {
    return catalog.embed[0];
  }
  return null;
}

/** Apply detected capabilities onto live CFG (in-memory; file unchanged). */
export async function bootstrapRuntime({ force = false } = {}) {
  if (runtime.bootstrapped && !force) {
    return runtimeSummary();
  }

  runtime.notes = [];
  const discovered = await discoverLmStudio(CFG.lmBaseUrl);
  if (!discovered.ok) {
    runtime.notes.push("lm offline");
  } else {
    if (discovered.baseUrl !== CFG.lmBaseUrl) {
      runtime.notes.push(`lm → ${discovered.baseUrl}`);
    }
    CFG.lmBaseUrl = discovered.baseUrl;
    CFG.openaiBaseUrl = discovered.baseUrl;
  }

  const [stt, catalog] = await Promise.all([
    refreshSttCapability({ force }),
    discovered.ok ? fetchOpenAiModels() : Promise.resolve({ ok: false, error: "lm offline" }),
  ]);

  runtime.stt = stt;
  runtime.models = catalog;

  let cal = null;
  try {
    const db = openDb();
    cal = applyCalibrationToCfg(CFG, db);
    if (cal.calibrated) {
      runtime.notes.push(
        `gates speech=${CFG.speechEnergyThreshold.toFixed(4)} motion=${CFG.perceptionMotionMin.toFixed(3)}`,
      );
    } else {
      runtime.notes.push("gates open (calibrating from ingest)");
    }
  } catch (err) {
    runtime.notes.push(`calibration: ${err.message}`);
  }

  if (catalog.ok) {
    try {
      const savedModel = userConfigOverrides().lm?.model;
      const fast = requireChatModel(catalog, savedModel ?? CFG.modelFast);
      CFG.modelFast = fast;
      CFG.modelDeep = fast;
      if (!savedModel) {
        runtime.notes.push(`model → ${fast}`);
      }

      const embed = requireEmbedModel(catalog, CFG.lmModelEmbed);
      if (embed && embed !== CFG.lmModelEmbed) {
        runtime.notes.push(`embed → ${embed}`);
      }
      CFG.lmModelEmbed = embed;
    } catch (err) {
      runtime.notes.push(err.message);
    }
  }

  const embedReady = Boolean(CFG.lmModelEmbed && catalog.ok && catalog.embed?.includes(CFG.lmModelEmbed));
  runtime.embeddings = embedReady;
  CFG.memoryUseEmbeddingsEffective = embedReady;

  if (embedReady) {
    runtime.notes.push("memory: embeddings");
  }

  if (stt.ready) {
    runtime.notes.push(`stt → cli ${stt.model} · ${stt.device}`);
  } else if (CFG.audioSttEnabled !== false) {
    runtime.notes.push(`stt: whisper CLI missing (${CFG.whisperBin})`);
  }

  runtime.calibration = cal;

  runtime.bootstrapped = true;
  return runtimeSummary();
}

export function memoryEmbeddingsActive() {
  if (CFG.memoryUseEmbeddings === false) {
    return false;
  }
  return CFG.memoryUseEmbeddingsEffective ?? Boolean(CFG.lmModelEmbed);
}

export function runtimeSummary() {
  return {
    ...runtime,
    lm: {
      url: CFG.lmBaseUrl,
      fast: CFG.modelFast,
      deep: CFG.modelDeep,
      embed: CFG.lmModelEmbed,
    },
    stt: runtime.stt ?? {},
    embeddings: memoryEmbeddingsActive(),
  };
}

export function adaptivePipelineIntervalMs(pendingTotal = 0) {
  const base = CFG.pipelineIntervalMs ?? 30_000;
  if (pendingTotal <= 0) {
    return base;
  }
  if (pendingTotal > 100) {
    return Math.max(2000, Math.floor(base / 6));
  }
  if (pendingTotal > 20) {
    return Math.max(4000, Math.floor(base / 3));
  }
  return Math.max(8000, Math.floor(base / 2));
}

export function derivedPresentGaps() {
  const chunk = CFG.audioChunkMs ?? 1000;
  const frameMs = CFG.frameCaptureIntervalMs ?? 60_000;
  return {
    speechGapMs: CFG.presentSpeechGapMs ?? Math.max(30_000, chunk * 40),
    sceneGapMs: CFG.presentSceneGapMs ?? Math.max(120_000, frameMs * 6),
    soundGapMs: CFG.presentSoundGapMs ?? Math.max(15_000, chunk * 15),
  };
}

/** After config hot-reload from API/UI. */
export async function onConfigReloaded() {
  reloadConfig();
  return bootstrapRuntime({ force: true });
}
