import { LEXICAL_MIN_TOKEN_LENGTH, lexicalStopWordSet } from "../locale.mjs";

export function tokenize(text) {
  const stop = lexicalStopWordSet();
  return String(text ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^a-z0-9à-ú]+/gi, " ")
    .split(/\s+/)
    .filter((t) => t.length >= LEXICAL_MIN_TOKEN_LENGTH && !stop.has(t));
}

export function termVector(text) {
  const vec = new Map();
  for (const tok of tokenize(text)) {
    vec.set(tok, (vec.get(tok) ?? 0) + 1);
  }
  return vec;
}

export function cosineSimilarity(a, b) {
  if (!a.size || !b.size) {
    return 0;
  }
  let dot = 0;
  let normA = 0;
  let normB = 0;
  for (const v of a.values()) {
    normA += v * v;
  }
  for (const v of b.values()) {
    normB += v * v;
  }
  const smaller = a.size < b.size ? a : b;
  const larger = a.size < b.size ? b : a;
  for (const [k, v] of smaller) {
    const w = larger.get(k);
    if (w) {
      dot += v * w;
    }
  }
  if (!normA || !normB) {
    return 0;
  }
  return dot / (Math.sqrt(normA) * Math.sqrt(normB));
}

export function lexicalScore(queryText, docText) {
  return cosineSimilarity(termVector(queryText), termVector(docText));
}
