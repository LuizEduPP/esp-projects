import { createWriteStream, existsSync, mkdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { pipeline } from "node:stream/promises";
import { CFG, PATHS } from "../config.mjs";

let sessionPromise = null;
let labelNames = null;

function soundLabel(kind) {
  return CFG.perceptionSoundLabels?.[kind] ?? kind;
}

function pcmToFloat32(pcmBuffer) {
  const samples = new Int16Array(
    pcmBuffer.buffer,
    pcmBuffer.byteOffset,
    pcmBuffer.length / 2,
  );
  const out = new Float32Array(samples.length);
  for (let i = 0; i < samples.length; i++) {
    out[i] = samples[i] / 32768;
  }
  return out;
}

async function downloadFile(url, dest) {
  mkdirSync(PATHS.modelsDir(), { recursive: true });
  const res = await fetch(url);
  if (!res.ok) {
    throw new Error(`download failed ${url}: ${res.status}`);
  }
  await pipeline(res.body, createWriteStream(dest));
}

async function ensureAsset(path, url, filename) {
  if (path && existsSync(path)) {
    return path;
  }
  const dest = join(PATHS.modelsDir(), filename);
  if (!existsSync(dest)) {
    if (!url) {
      throw new Error(`missing ${filename} — set path or url in config`);
    }
    console.log(`[yamnet] downloading ${filename}…`);
    await downloadFile(url, dest);
  }
  return dest;
}

async function loadLabels() {
  if (labelNames) {
    return labelNames;
  }
  const path = await ensureAsset(
    CFG.perceptionYamnetLabelsPath,
    CFG.perceptionYamnetLabelsUrl,
    "yamnet_class_map.csv",
  );
  const lines = readFileSync(path, "utf8").trim().split("\n");
  const names = [];
  for (let i = 1; i < lines.length; i++) {
    const line = lines[i];
    const match = line.match(/^(\d+),([^,]*),"?([^"]*)"?$/);
    if (match) {
      names[Number(match[1])] = match[3];
    }
  }
  labelNames = names;
  return names;
}

function aggregateScores(data, dims) {
  const classCount = labelsLength(dims);
  const aggregated = new Float32Array(classCount);
  if (dims.length === 1) {
    for (let i = 0; i < Math.min(classCount, data.length); i++) {
      aggregated[i] = data[i];
    }
    return aggregated;
  }

  const frames = dims.length === 3 ? dims[1] : dims[0];
  const classes = dims.length === 3 ? dims[2] : dims[1];
  for (let f = 0; f < frames; f++) {
    for (let c = 0; c < classes; c++) {
      aggregated[c] += data[f * classes + c];
    }
  }
  if (frames > 1) {
    for (let c = 0; c < classes; c++) {
      aggregated[c] /= frames;
    }
  }
  return aggregated;
}

function labelsLength(dims) {
  if (dims.length === 1) {
    return dims[0];
  }
  if (dims.length === 2) {
    return dims[1];
  }
  if (dims.length === 3) {
    return dims[2];
  }
  return 521;
}

async function loadSession() {
  if (!sessionPromise) {
    sessionPromise = (async () => {
      const ort = await import("onnxruntime-node");
      const modelPath = await ensureAsset(
        CFG.perceptionYamnetModelPath,
        CFG.perceptionYamnetModelUrl,
        "yamnet.onnx",
      );
      return ort.InferenceSession.create(modelPath, { executionProviders: ["cpu"] });
    })();
  }
  return sessionPromise;
}

function kindFromClassName(name) {
  const map = CFG.perceptionYamnetKindMap ?? {};
  const lower = String(name ?? "").toLowerCase();
  for (const [kind, patterns] of Object.entries(map)) {
    for (const pattern of patterns) {
      if (lower.includes(String(pattern).toLowerCase())) {
        return kind;
      }
    }
  }
  return "unknown";
}

export function resetYamnetSession() {
  sessionPromise = null;
  labelNames = null;
}

export async function yamnetTopClasses(pcmBuffer) {
  const session = await loadSession();
  const labels = await loadLabels();
  const waveform = pcmToFloat32(pcmBuffer);
  const ort = await import("onnxruntime-node");
  const inputName = session.inputNames[0] ?? "waveform";
  const tensor = new ort.Tensor("float32", waveform, [waveform.length]);
  const result = await session.run({ [inputName]: tensor });
  const outputName =
    session.outputNames.find((n) => /score|logit|output/i.test(n)) ?? session.outputNames[0];
  const out = result[outputName];
  if (!out?.data?.length) {
    throw new Error("yamnet produced no scores");
  }

  const aggregated = aggregateScores(out.data, out.dims ?? [out.data.length]);
  const ranked = [];
  for (let i = 0; i < aggregated.length; i++) {
    if (labels[i]) {
      ranked.push({ index: i, name: labels[i], score: aggregated[i] });
    }
  }
  ranked.sort((a, b) => b.score - a.score);
  return ranked.slice(0, 8);
}

export async function classifySoundYamnet(pcmBuffer, energy) {
  const ranked = await yamnetTopClasses(pcmBuffer);
  const minScore = CFG.perceptionYamnetMinScore;
  const top = ranked.find((r) => r.score >= minScore) ?? ranked[0];
  if (!top || top.score < minScore) {
    return {
      kind: "unknown",
      label: soundLabel("unknown"),
      confidence: top?.score ?? 0,
      yamnet: ranked.slice(0, 3),
    };
  }

  if (energy >= CFG.speechEnergyThreshold && kindFromClassName(top.name) === "unknown") {
    return {
      kind: "speech",
      label: soundLabel("speech"),
      confidence: Math.max(top.score, 0.5),
      yamnet: ranked.slice(0, 3),
    };
  }

  const kind = kindFromClassName(top.name);
  return {
    kind,
    label: soundLabel(kind),
    confidence: top.score,
    yamnet: ranked.slice(0, 3),
  };
}
