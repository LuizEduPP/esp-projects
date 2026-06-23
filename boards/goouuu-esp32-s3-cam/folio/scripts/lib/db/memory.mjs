import { CFG } from "../config/index.mjs";
import { dayOffset, isoNow } from "../util/time.mjs";

export function deleteMemoryForDay(db, day) {
  db.prepare("DELETE FROM memory_chunks WHERE day = ?").run(day);
}

export function insertMemoryChunk(db, row) {
  db.prepare(
    `INSERT INTO memory_chunks (day, kind, text, evidence_json, embedding_json, entity_id, weight, created_at)
     VALUES (@day, @kind, @text, @evidence_json, @embedding_json, @entity_id, @weight, @created_at)`,
  ).run({
    day: row.day,
    kind: row.kind,
    text: row.text,
    evidence_json: row.evidence_json ?? null,
    embedding_json: row.embedding_json ?? null,
    entity_id: row.entity_id ?? null,
    weight: row.weight ?? 1,
    created_at: row.created_at,
  });
}

export function memoryChunksInRange(db, minDay, beforeDay) {
  return db
    .prepare(`SELECT * FROM memory_chunks WHERE day >= ? AND day < ? ORDER BY day DESC`)
    .all(minDay, beforeDay);
}

export function memoryChunkCount(db) {
  return db.prepare("SELECT COUNT(*) AS n FROM memory_chunks").get().n;
}

export function memoryChunksForEntity(db, entityId, limit = 20) {
  return db
    .prepare(
      `SELECT * FROM memory_chunks WHERE entity_id = ? ORDER BY day DESC, id DESC LIMIT ?`,
    )
    .all(entityId, limit);
}

export function profileFactsFromMemory(db, limit = CFG.memoryProfileLimit) {
  return db
    .prepare(
      `SELECT entity_id, kind, text, day FROM memory_chunks
       WHERE entity_id IS NOT NULL
       ORDER BY day DESC, id DESC LIMIT ?`,
    )
    .all(limit);
}
