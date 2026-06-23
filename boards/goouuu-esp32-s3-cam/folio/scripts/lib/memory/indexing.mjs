import { CFG } from "../config/index.mjs";
import {
  deleteMemoryForDay,
  getSpeaker,
  insertMemoryChunk,
  openDb,
  timelineForDay,
} from "../db/index.mjs";
import { embedText, serializeEmbedding } from "./embeddings.mjs";

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
        entity_id: item.sound_kind === "bark" ? "pet:dog" : null,
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
