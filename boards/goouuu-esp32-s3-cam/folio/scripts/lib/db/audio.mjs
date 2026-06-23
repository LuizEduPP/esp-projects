import { existsSync, unlinkSync } from "node:fs";
import { CFG } from "../config/index.mjs";
import { dayBounds, retentionCutoffIso } from "../util/time.mjs";

function deletePcmFile(path) {
  if (!path || !existsSync(path)) {
    return;
  }
  try {
    unlinkSync(path);
  } catch {
    /* ignore */
  }
}

export function clearAudioChunkPath(db, id) {
  db.prepare("UPDATE audio_chunks SET path = '' WHERE id = ?").run(id);
}

export function insertAudioChunk(db, row) {
  const info = db
    .prepare(
      `INSERT INTO audio_chunks (device_id, captured_at, seq, path, duration_ms, energy, device_ms)
       VALUES (@device_id, @captured_at, @seq, @path, @duration_ms, @energy, @device_ms)`,
    )
    .run({
      device_ms: null,
      ...row,
    });
  return Number(info.lastInsertRowid);
}

export function pendingAudioChunks(db, limit = 20) {
  return db
    .prepare(
      `SELECT * FROM audio_chunks WHERE processed = 0
       ORDER BY CASE WHEN energy >= ? THEN 0 ELSE 1 END, captured_at ASC
       LIMIT ?`,
    )
    .all(CFG.speechEnergyThreshold, limit);
}

export function markAudioProcessed(db, id) {
  db.prepare("UPDATE audio_chunks SET processed = 1 WHERE id = ?").run(id);
}

export function bumpSttAttempts(db, id) {
  db.prepare("UPDATE audio_chunks SET stt_attempts = stt_attempts + 1 WHERE id = ?").run(id);
  return db.prepare("SELECT stt_attempts FROM audio_chunks WHERE id = ?").get(id)?.stt_attempts ?? 0;
}

export function deleteAudioChunk(db, id) {
  const row = db.prepare("SELECT path FROM audio_chunks WHERE id = ?").get(id);
  deletePcmFile(row?.path);
  db.prepare("DELETE FROM audio_chunks WHERE id = ?").run(id);
}

export function pruneExpiredPcm(db, retentionDays = CFG.audioRetentionDays) {
  if (retentionDays <= 0) {
    return { files: 0 };
  }
  const cutoff = retentionCutoffIso(retentionDays);
  const rows = db
    .prepare(
      `SELECT id, path FROM audio_chunks
       WHERE path != '' AND captured_at < ?`,
    )
    .all(cutoff);
  let files = 0;
  for (const row of rows) {
    deletePcmFile(row.path);
    clearAudioChunkPath(db, row.id);
    files++;
  }
  return { files };
}
