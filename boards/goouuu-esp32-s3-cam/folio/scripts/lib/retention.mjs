import { existsSync, unlinkSync } from "node:fs";
import { CFG } from "./config.mjs";
import { openDb } from "./db.mjs";

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

/** Drop unprocessed quiet chunks (legacy ingest before storeQuiet=false). */
export function purgeUnprocessedQuiet(db = openDb()) {
  const rows = db
    .prepare(
      `SELECT id, path FROM audio_chunks
       WHERE processed = 0 AND energy < ?`,
    )
    .all(CFG.speechEnergyThreshold);
  const files = deleteChunkRows(db, rows);
  return { files };
}

/** Remove processed silence files and stale rows. */
export function pruneQuietAudio(db = openDb()) {
  const rows = db
    .prepare(
      `SELECT id, path FROM audio_chunks
       WHERE processed = 1 AND energy < ? AND path IS NOT NULL AND path != ''`,
    )
    .all(CFG.speechEnergyThreshold);
  const files = deleteChunkRows(db, rows);
  return { files, rowsDeleted: files };
}

/** Drop old processed audio without utterances (keep speech with transcripts). */
export function pruneOldAudio(db = openDb()) {
  const cutoff = new Date(Date.now() - CFG.audioRetentionDays * 86400000).toISOString();
  const rows = db
    .prepare(
      `SELECT c.id, c.path FROM audio_chunks c
       LEFT JOIN utterances u ON u.chunk_id = c.id
       WHERE c.processed = 1 AND u.id IS NULL AND c.captured_at < ?`,
    )
    .all(cutoff);
  const files = deleteChunkRows(db, rows);
  return { files, cutoff };
}

export function runRetentionPass() {
  const db = openDb();
  const legacy = purgeUnprocessedQuiet(db);
  const quiet = pruneQuietAudio(db);
  const old = pruneOldAudio(db);
  const total = legacy.files + quiet.files + old.files;
  if (total > 0) {
    console.log(
      `[retention] removed ${legacy.files} pending-quiet + ${quiet.files} quiet + ${old.files} stale ` +
        `(keep ${CFG.audioRetentionDays}d)`,
    );
  }
  return { legacy, quiet, old };
}

const RETENTION_INTERVAL_MS = 6 * 60 * 60 * 1000;

export function startRetentionLoop() {
  runRetentionPass();
  setInterval(runRetentionPass, RETENTION_INTERVAL_MS);
}
