import { CFG } from "../config/index.mjs";

let stopSetCache = null;
let stopSetKey = null;

function stopWordsSet() {
  const words = CFG.memoryLexicalStopWords;
  const key = words.join("\0");
  if (stopSetKey !== key) {
    stopSetCache = new Set(words.map((w) => String(w).toLowerCase()));
    stopSetKey = key;
  }
  return stopSetCache;
}

export function tokenize(text) {
  const minLen = CFG.memoryLexicalMinTokenLength;
  const stops = stopWordsSet();
  return String(text ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .split(/[^a-z0-9]+/)
    .filter((t) => t.length >= minLen && !stops.has(t));
}

export function termVector(text) {
  const vec = new Map();
  for (const tok of tokenize(text)) {
    vec.set(tok, (vec.get(tok) ?? 0) + 1);
  }
  return vec;
}

export function cosineSimilarity(a, b) {
  let dot = 0;
  let na = 0;
  let nb = 0;
  for (const v of a.values()) {
    na += v * v;
  }
  for (const v of b.values()) {
    nb += v * v;
  }
  const keys = a.size < b.size ? a.keys() : b.keys();
  for (const k of keys) {
    dot += (a.get(k) ?? 0) * (b.get(k) ?? 0);
  }
  if (!na || !nb) {
    return 0;
  }
  return dot / (Math.sqrt(na) * Math.sqrt(nb));
}

export function cosineDense(a, b) {
  if (!Array.isArray(a) || !Array.isArray(b) || a.length !== b.length) {
    return 0;
  }
  let dot = 0;
  let na = 0;
  let nb = 0;
  for (let i = 0; i < a.length; i++) {
    dot += a[i] * b[i];
    na += a[i] * a[i];
    nb += b[i] * b[i];
  }
  if (!na || !nb) {
    return 0;
  }
  return dot / (Math.sqrt(na) * Math.sqrt(nb));
}

export function vectorFromJson(json) {
  if (!json) {
    return new Map();
  }
  try {
    const parsed = JSON.parse(json);
    if (Array.isArray(parsed)) {
      if (parsed.length && Array.isArray(parsed[0])) {
        return new Map(parsed);
      }
      return parsed;
    }
    if (Array.isArray(parsed?.vector)) {
      return parsed.kind === "lexical" ? new Map(parsed.vector) : parsed.vector;
    }
  } catch {
    /* ignore */
  }
  return new Map();
}
