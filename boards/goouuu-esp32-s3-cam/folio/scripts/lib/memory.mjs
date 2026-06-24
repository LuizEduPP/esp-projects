import { CFG } from "./config.mjs";
import { isMeaningfulFrameItem } from "./present.mjs";
import {
  deleteMemoryForDay,
  ensureSpeakerEntity,
  entityBySpeakerId,
  getEntity,
  getSpeaker,
  insertMemoryChunk,
  memoryChunksInRange,
  memoryEvidenceSet,
  openDb,
  speakerEntityId,
  timelineForDay,
} from "./db.mjs";
import { createEmbeddings } from "./llm.mjs";
import { modelId, ModelSlot } from "./models.mjs";
import { dayOffset, isDarkSceneCaption, isSttHallucination } from "./util.mjs";


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
const UTTERANCE_FILLER = new Set([
  "ok",
  "okay",
  "sim",
  "nao",
  "não",
  "obrigado",
  "obrigada",
  "valeu",
  "e agora",
  "e ai",
  "e aí",
  "ve",
  "no",
  "but",
  "yeah",
  "yes",
  "no",
  "thanks",
  "thank you",
  "alright",
  "simply",
]);

function cleanMemoryText(text) {
  return String(text ?? "")
    .replace(/<\|[^|>]+\|>/g, " ")
    .replace(/\s+/g, " ")
    .trim();
}

function isDarkHallucinationCaption(caption) {
  return isDarkSceneCaption(caption);
}

function isMemoryWorthyText(text, kind) {
  const t = cleanMemoryText(text);
  if (isSttHallucination(t, CFG.audioSttRejectPatterns)) {
    return false;
  }
  const minLen = kind === "utterance" ? (CFG.memoryMinUtteranceChars ?? 12) : 8;
  if (t.length < minLen) {
    return false;
  }
  const norm = t
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^\p{L}\p{N}\s]/gu, "")
    .trim();
  if (UTTERANCE_FILLER.has(norm)) {
    return false;
  }
  const tokens = norm.split(/\s+/).filter(Boolean);
  if (tokens.length >= 2 && new Set(tokens).size === 1) {
    return false;
  }
  if (kind === "utterance" && /^(\w+\s*){1,2}$/.test(norm) && norm.length < 16) {
    return false;
  }
  return true;
}

export async function embedText(text) {
  const [embed] = await embedTexts([text]);
  return embed;
}

export async function embedTexts(texts) {
  const cleaned = texts.map((t) => cleanMemoryText(t).slice(0, 8192));
  if (!CFG.memoryUseEmbeddings) {
    return cleaned.map((t) => ({ kind: "lexical", vector: [...termVector(t).entries()] }));
  }
  const model = modelId(ModelSlot.EMBED);
  if (!model) {
    throw new Error("lm.modelEmbed not set — load an embedding model in LM Studio");
  }
  const batchSize = Math.max(1, CFG.memoryEmbedBatchSize ?? 32);
  const out = [];
  for (let i = 0; i < cleaned.length; i += batchSize) {
    const slice = cleaned.slice(i, i + batchSize);
    const json = await createEmbeddings({
      model,
      input: slice.length === 1 ? slice[0] : slice,
      encoding_format: "float",
    });
    const rows = [...(json?.data ?? [])].sort((a, b) => (a.index ?? 0) - (b.index ?? 0));
    if (rows.length !== slice.length) {
      throw new Error(`embedding batch: expected ${slice.length} vectors, got ${rows.length}`);
    }
    for (const row of rows) {
      const vec = row.embedding;
      if (!Array.isArray(vec)) {
        throw new Error("no embedding vector");
      }
      out.push({ kind: "float", vector: vec });
    }
  }
  return out;
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
function flushUtteranceGroup(group, out, day) {
  if (!group.length) {
    return;
  }
  const parts = group.map((g) => g.text);
  const merged = parts.join(" · ");
  if (!isMemoryWorthyText(merged, "utterance")) {
    return;
  }
  const first = group[0];
  const last = group[group.length - 1];
  out.push({
    day,
    kind: "utterance",
    text: merged,
    evidence_json: JSON.stringify({
      utterance_ids: group.map((g) => g.utterance_id).filter(Boolean),
      chunk_ids: group.map((g) => g.chunk_id).filter(Boolean),
      at: first.at,
      through: last.at,
    }),
    entity_id: first.entity_id ?? null,
    weight: 1.2,
  });
}

export function collectMemoryItems(db, day) {
  const items = timelineForDay(db, day);
  const out = [];
  const speakerCache = new Map();
  const groupMs = Math.max(60_000, CFG.memoryUtteranceGroupMs ?? 300_000);
  let utteranceGroup = [];
  let groupStartMs = null;

  const pushUtterance = (row) => {
    const atMs = new Date(row.at).getTime();
    if (
      utteranceGroup.length &&
      (Number.isNaN(atMs) ||
        atMs - groupStartMs > groupMs ||
        (row.entity_id && row.entity_id !== utteranceGroup[0].entity_id))
    ) {
      flushUtteranceGroup(utteranceGroup, out, day);
      utteranceGroup = [];
      groupStartMs = null;
    }
    if (!utteranceGroup.length) {
      groupStartMs = atMs;
    }
    utteranceGroup.push(row);
  };

  for (const item of items) {
    if (item.type === "frame" && isMeaningfulFrameItem(item)) {
      flushUtteranceGroup(utteranceGroup, out, day);
      utteranceGroup = [];
      groupStartMs = null;

      if (isDarkHallucinationCaption(item.caption)) {
        continue;
      }
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
      let entityId = null;
      if (item.speaker_id) {
        if (!speakerCache.has(item.speaker_id)) {
          speakerCache.set(item.speaker_id, getSpeaker(db, item.speaker_id));
        }
        speakerName = speakerCache.get(item.speaker_id)?.display_name ?? item.speaker_id;
        entityId =
          entityBySpeakerId(db, item.speaker_id)?.id ??
          getEntity(db, speakerEntityId(item.speaker_id))?.id ??
          ensureSpeakerEntity(db, item.speaker_id, speakerName);
      }
      const prefix = speakerName ? `${speakerName}: ` : "";
      const text = cleanMemoryText(`${prefix}${item.text}`);
      if (!isMemoryWorthyText(text, "utterance")) {
        continue;
      }
      pushUtterance({
        text,
        at: item.at,
        utterance_id: item.utterance_id,
        chunk_id: item.chunk_id,
        entity_id: entityId,
      });
      continue;
    }

    if (item.sound_label || item.sound_kind) {
      flushUtteranceGroup(utteranceGroup, out, day);
      utteranceGroup = [];
      groupStartMs = null;

      const text = `${item.sound_label || item.sound_kind} (E=${Number(item.energy ?? 0).toFixed(4)})`;
      if (!isMemoryWorthyText(text, "sound")) {
        continue;
      }
      out.push({
        day,
        kind: item.sound_kind || "sound",
        text,
        evidence_json: JSON.stringify({ chunk_id: item.chunk_id, at: item.at }),
        entity_id: CFG.entitiesSoundKindEntity?.[item.sound_kind] ?? null,
        weight: 0.7,
      });
    }
  }

  flushUtteranceGroup(utteranceGroup, out, day);
  return out;
}

function pruneLegacyMemoryChunks(db, day) {
  const rows = db.prepare("SELECT id, evidence_json FROM memory_chunks WHERE day = ?").all(day);
  let pruned = 0;
  for (const row of rows) {
    try {
      const ev = JSON.parse(row.evidence_json || "{}");
      if (ev.utterance_id != null && ev.utterance_ids == null) {
        db.prepare("DELETE FROM memory_chunks WHERE id = ?").run(row.id);
        pruned++;
      }
    } catch {
      /* ignore */
    }
  }
  return pruned;
}

function pruneJunkMemoryChunks(db, day) {
  const rows = db.prepare("SELECT id, text FROM memory_chunks WHERE day = ?").all(day);
  let pruned = 0;
  for (const row of rows) {
    if (
      isSttHallucination(row.text, CFG.audioSttRejectPatterns) ||
      isDarkSceneCaption(row.text)
    ) {
      db.prepare("DELETE FROM memory_chunks WHERE id = ?").run(row.id);
      pruned++;
    }
  }
  return pruned;
}

export async function indexDayMemories(db, day, { force = false } = {}) {
  if (!CFG.memoryEnabled) {
    return { indexed: 0, skipped: 0 };
  }

  if (force) {
    deleteMemoryForDay(db, day);
  } else {
    const pruned = pruneLegacyMemoryChunks(db, day);
    if (pruned) {
      console.log(`[memory] pruned ${pruned} legacy chunks for ${day}`);
    }
    const junk = pruneJunkMemoryChunks(db, day);
    if (junk) {
      console.log(`[memory] pruned ${junk} junk chunks for ${day}`);
    }
  }

  const items = collectMemoryItems(db, day);
  const existing = force ? new Set() : memoryEvidenceSet(db, day);
  const pending = items.filter((item) => !existing.has(item.evidence_json));
  if (!pending.length) {
    console.log(`[memory] ${day} up to date (${items.length} chunks)`);
    return { indexed: 0, skipped: items.length };
  }

  const embeds = await embedTexts(pending.map((item) => item.text));
  const now = new Date().toISOString();
  let indexed = 0;
  for (let i = 0; i < pending.length; i++) {
    insertMemoryChunk(db, {
      ...pending[i],
      embedding_json: serializeEmbedding(embeds[i]),
      created_at: now,
    });
    indexed++;
  }

  const skipped = items.length - indexed;
  console.log(
    `[memory] indexed ${indexed} new for ${day}` +
      (skipped ? ` (${skipped} already indexed)` : "") +
      (CFG.memoryUseEmbeddings
        ? ` · ${Math.ceil(indexed / Math.max(1, CFG.memoryEmbedBatchSize ?? 32))} embed batch(es)`
        : ""),
  );
  return { indexed, skipped };
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
    const { indexed } = await indexDayMemories(db, day, { force: true });
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
