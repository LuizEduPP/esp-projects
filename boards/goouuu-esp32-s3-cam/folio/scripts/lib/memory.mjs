import { CFG } from "./config.mjs";
import { deleteMemoryForDay, getSpeaker, insertMemoryChunk, memoryChunksInRange, openDb, timelineForDay } from "./db.mjs";
import { createEmbeddings } from "./llm.mjs";
import { modelId, ModelSlot } from "./models.mjs";
import { dayOffset } from "./util.mjs";


// --- lexical.mjs ---
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

// --- embeddings.mjs ---
export async function embedText(text) {
  if (!CFG.memoryUseEmbeddings) {
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }
  const json = await createEmbeddings({
    model: modelId(ModelSlot.EMBED),
    input: text.slice(0, 8192),
    encoding_format: "float",
  });
  const vec = json?.data?.[0]?.embedding;
  if (!Array.isArray(vec)) {
    throw new Error("no embedding vector");
  }
  return { kind: "float", vector: vec };
}

export function scorePair(queryEmbed, docEmbedJson) {
  const stored = vectorFromJson(docEmbedJson);

  if (queryEmbed.kind === "float") {
    return Array.isArray(stored) ? cosineDense(queryEmbed.vector, stored) : 0;
  }

  if (stored instanceof Map) {
    return cosineSimilarity(new Map(queryEmbed.vector), stored);
  }

  return 0;
}

export function serializeEmbedding(embed) {
  return JSON.stringify(embed.vector);
}

// --- indexing.mjs ---
export function collectMemoryItems(db, day) {
  const items = timelineForDay(db, day);
  const out = [];
  const speakerCache = new Map();

  for (const item of items) {
    if (item.type === "frame" && item.caption) {
      out.push({
        day,
        kind: "frame",
        text: item.caption,
        evidence_json: JSON.stringify({ frame_id: item.frame_id, at: item.at }),
        entity_id: null,
        weight: 0.9,
      });
      continue;
    }

    if (item.type !== "audio") {
      continue;
    }

    if (item.text) {
      let speakerName = null;
      let entityId = item.speaker_id ?? null;
      if (item.speaker_id) {
        if (!speakerCache.has(item.speaker_id)) {
          speakerCache.set(item.speaker_id, getSpeaker(db, item.speaker_id));
        }
        speakerName = speakerCache.get(item.speaker_id)?.display_name ?? item.speaker_id;
      }
      const prefix = speakerName ? `${speakerName}: ` : "";
      out.push({
        day,
        kind: "utterance",
        text: `${prefix}${item.text}`,
        evidence_json: JSON.stringify({
          utterance_id: item.utterance_id,
          chunk_id: item.chunk_id,
          at: item.at,
          speaker_id: item.speaker_id,
        }),
        entity_id: entityId,
        weight: 1.2,
      });
      continue;
    }

    if (item.sound_label || item.sound_kind) {
      out.push({
        day,
        kind: item.sound_kind || "sound",
        text: `${item.sound_label || item.sound_kind} (E=${Number(item.energy ?? 0).toFixed(4)})`,
        evidence_json: JSON.stringify({ chunk_id: item.chunk_id, at: item.at }),
        entity_id: CFG.entitiesSoundKindEntity?.[item.sound_kind] ?? null,
        weight: 0.7,
      });
    }
  }

  return out;
}

export async function indexDayMemories(db, day) {
  if (!CFG.memoryEnabled) {
    return { indexed: 0 };
  }

  const items = collectMemoryItems(db, day);
  deleteMemoryForDay(db, day);

  let indexed = 0;
  for (const item of items) {
    const embed = await embedText(item.text);
    insertMemoryChunk(db, {
      ...item,
      embedding_json: serializeEmbedding(embed),
      created_at: new Date().toISOString(),
    });
    indexed++;
  }

  console.log(`[memory] indexed ${indexed} chunks for ${day}`);
  return { indexed };
}

export async function reindexAllMemories(db = openDb()) {
  const days = db
    .prepare(
      `SELECT DISTINCT substr(captured_at, 1, 10) AS day FROM audio_chunks
       UNION SELECT DISTINCT substr(captured_at, 1, 10) FROM frames
       ORDER BY day`,
    )
    .all()
    .map((r) => r.day)
    .filter(Boolean);

  let total = 0;
  for (const day of days) {
    const { indexed } = await indexDayMemories(db, day);
    total += indexed;
  }
  return { days: days.length, chunks: total };
}

// --- retrieval.mjs ---
export async function retrieveMemories(db, query, { day, limit = CFG.memoryRetrieveLimit } = {}) {
  if (!query?.trim()) {
    return [];
  }

  const beforeDay = day ?? new Date().toISOString().slice(0, 10);
  const minDay = dayOffset(beforeDay, -CFG.memoryLookbackDays);
  const candidates = memoryChunksInRange(db, minDay, beforeDay);

  const queryEmbed = await embedText(query);
  const scored = candidates
    .map((c) => ({
      ...c,
      score: scorePair(queryEmbed, c.embedding_json),
    }))
    .filter((c) => c.score >= CFG.memoryMinScore)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit);

  return scored;
}

export async function retrieveContextForDay(db, day, { query = "", limit = CFG.memoryRetrieveLimit } = {}) {
  const q =
    query ||
    String(CFG.memoryContextQueryTemplate ?? "").replaceAll("{day}", day).trim();
  return retrieveMemories(db, q, { day, limit });
}
