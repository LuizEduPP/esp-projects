import { existsSync, unlinkSync } from "node:fs";
import { CFG } from "./config.mjs";
import { deleteAudioChunk, openDb } from "./db.mjs";

export function deleteChunkFile(path) {
  if (!path || !existsSync(path)) {
    return;
  }
  try {
    unlinkSync(path);
  } catch {
    /* ignore */
  }
}

/** Remove PCM file and DB row — used for silence / empty STT. */
export function discardAudioChunk(db, chunk) {
  deleteChunkFile(chunk.path);
  deleteAudioChunk(db, chunk.id);
}

function deleteChunkRows(db, rows) {
  if (!rows.length) {
    return 0;
  }
  for (const row of rows) {
    deleteChunkFile(row.path);
  }
  const batchSize = 400;
  for (let i = 0; i < rows.length; i += batchSize) {
    const batch = rows.slice(i, i + batchSize);
    const placeholders = batch.map(() => "?").join(",");
    db.prepare(`DELETE FROM audio_chunks WHERE id IN (${placeholders})`).run(
      ...batch.map((r) => r.id),
    );
  }
  return rows.length;
}

/** Legacy quiet rows + processed chunks without transcripts. */
export function pruneStaleAudio(db = openDb()) {
  const quiet = db
    .prepare(
      `SELECT id, path FROM audio_chunks
       WHERE energy < ? AND (
         processed = 0
         OR (processed = 1 AND path IS NOT NULL AND path != '')
       )`,
    )
    .all(CFG.speechEnergyThreshold);

  const orphan = db
    .prepare(
      `SELECT c.id, c.path FROM audio_chunks c
       LEFT JOIN utterances u ON u.chunk_id = c.id
       WHERE c.processed = 1 AND u.id IS NULL`,
    )
    .all();

  const seen = new Set();
  const rows = [];
  for (const row of [...quiet, ...orphan]) {
    if (!seen.has(row.id)) {
      seen.add(row.id);
      rows.push(row);
    }
  }

  return { files: deleteChunkRows(db, rows) };
}

export function runRetentionPass() {
  const db = openDb();
  const { files } = pruneStaleAudio(db);
  if (files > 0) {
    console.log(`[retention] removed ${files} stale audio chunks (keep transcripts ${CFG.audioRetentionDays}d)`);
  }
  return { files };
}

const RETENTION_INTERVAL_MS = 6 * 60 * 60 * 1000;

export function startRetentionLoop() {
  runRetentionPass();
  setInterval(runRetentionPass, RETENTION_INTERVAL_MS);
}
