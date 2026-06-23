import { dayBounds } from "../util.mjs";

export function insertUtterance(db, row) {
  const info = db
    .prepare(
      `INSERT INTO utterances (chunk_id, speaker_id, started_at, ended_at, text, confidence)
       VALUES (@chunk_id, @speaker_id, @started_at, @ended_at, @text, @confidence)`,
    )
    .run(row);
  return Number(info.lastInsertRowid);
}

export function getAudioChunk(db, id) {
  return db.prepare("SELECT * FROM audio_chunks WHERE id = ?").get(id);
}

export function audioChunksForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT c.*, u.id AS utterance_id, u.text AS utterance_text
       FROM audio_chunks c
       LEFT JOIN utterances u ON u.chunk_id = c.id
       WHERE c.captured_at >= ? AND c.captured_at < ?
       ORDER BY c.captured_at`,
    )
    .all(start, end);
}

export function utterancesForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT * FROM utterances WHERE started_at >= ? AND started_at < ? ORDER BY started_at`,
    )
    .all(start, end);
}
