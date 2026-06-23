import { existsSync, unlinkSync } from "node:fs";
import { CFG } from "../../config/index.mjs";
import {
  deleteAudioChunk,
  openDb,
  pendingAudioChunks,
  pendingCounts,
  pendingFrames,
  pruneExpiredPcm,
} from "../../db/index.mjs";
import { processAudioChunk, processFrame } from "../../perception/index.mjs";
let lastFrameLmAt = 0;
let loggedWhisperMissing = false;

/** Shorter gap when frames pile up — catch up without hammering LM at steady state. */
function frameCaptionGapMs(pendingFrameCount) {
  const base = CFG.frameCaptionIntervalMs;
  if (pendingFrameCount > 20) {
    return Math.min(base, 3000);
  }
  if (pendingFrameCount > 10) {
    return Math.min(base, 8000);
  }
  if (pendingFrameCount > 5) {
    return Math.min(base, 15000);
  }
  return base;
}

function deleteChunkFile(path) {
  if (!path || !existsSync(path)) {
    return;
  }
  try {
    unlinkSync(path);
  } catch {
    /* ignore */
  }
}

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
       WHERE c.processed = 1 AND u.id IS NULL AND c.path != ''
         AND c.energy >= ?`,
    )
    .all(CFG.speechEnergyThreshold);

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
  const stale = pruneStaleAudio(db);
  const expired = pruneExpiredPcm(db, CFG.audioRetentionDays);
  const total = stale.files + expired.files;
  if (total > 0) {
    console.log(
      `[retention] removed ${total} PCM file(s) ` +
        `(stale=${stale.files} expired>${CFG.audioRetentionDays}d=${expired.files}; transcripts kept)`,
    );
  }
  return { files: total, stale: stale.files, expired: expired.files };
}

export function startRetentionLoop() {
  runRetentionPass();
  setInterval(runRetentionPass, CFG.audioRetentionSweepMs);
}

function isWhisperError(err) {
  const msg = String(err?.message ?? err ?? "");
  return err?.code === "ENOENT" || /whisper/i.test(msg);
}

export async function processPendingAudio(limit = CFG.pipelineAudioBatch) {
  const db = openDb();
  const pending = pendingCounts(db);
  if (pending.audio > CFG.workerBacklogHigh) {
    limit = Math.min(CFG.workerBatchMaxHigh, limit * 3);
  } else if (pending.audio > CFG.workerBacklogMedium) {
    limit = Math.min(CFG.workerBatchMaxMedium, limit * 2);
  }

  const chunks = pendingAudioChunks(db, limit);
  const results = [];

  for (const chunk of chunks) {
    try {
      results.push(await processAudioChunk(db, chunk));
    } catch (err) {
      results.push({ id: chunk.id, error: err.message });
    }
  }

  return results;
}

export async function processPendingFrames(limit = CFG.pipelineFrameBatch, { bypassGap = false } = {}) {
  const db = openDb();
  const pending = pendingCounts(db);
  if (pending.frames > CFG.workerBacklogHigh) {
    limit = Math.min(CFG.workerBatchMaxHigh, limit * 2);
  } else if (pending.frames > CFG.workerBacklogMedium) {
    limit = Math.min(CFG.workerBatchMaxMedium, limit * 2);
  }

  const minGap = bypassGap ? 0 : frameCaptionGapMs(pending.frames);
  if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
    return [];
  }

  const frames = pendingFrames(db, limit);
  const results = [];

  for (const frame of frames) {
    if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
      break;
    }
    try {
      const result = await processFrame(db, frame);
      lastFrameLmAt = Date.now();
      results.push(result);
    } catch (err) {
      results.push({ id: frame.id, error: err.message });
      console.warn(`[perception] frame ${frame.id}: ${err.message}`);
    }
  }

  return results;
}

export async function runPendingQueueOnce({ bypassFrameGap = false } = {}) {
  const audio = await processPendingAudio();
  const frames = await processPendingFrames(CFG.pipelineFrameBatch, { bypassGap: bypassFrameGap });
  return { audio, frames };
}

export function startProcessingLoop(intervalMs = CFG.pipelineIntervalMs) {
  let busy = false;

  console.log(
    `[worker] every ${intervalMs}ms · audio batch=${CFG.pipelineAudioBatch} · ` +
      `frame batch=${CFG.pipelineFrameBatch} · LM gap=${CFG.frameCaptionIntervalMs}ms`,
  );

  return setInterval(async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const db = openDb();
      const pending = pendingCounts(db);
      if (pending.audio === 0 && pending.frames === 0) {
        return;
      }

      if (pending.audio > 0) {
        console.log(`[whisper] queue ${pending.audio} audio chunk(s)`);
      }
      if (pending.frames > 5) {
        console.log(`[worker] frame backlog ${pending.frames} — faster caption gap`);
      }

      const audio = await processPendingAudio();
      const frames = await processPendingFrames();

      const whisperErrors = audio.filter((r) => r.error && isWhisperError({ message: r.error }));
      if (whisperErrors.length && !loggedWhisperMissing) {
        loggedWhisperMissing = true;
        console.error(
          "[whisper] CLI not available — pip install openai-whisper or set audio.whisperBin in config",
        );
      }

      const utt = audio.filter((r) => r.text).length;
      const cap = frames.filter((r) => r.caption).length;
      const snd = audio.filter((r) => r.sound).length;
      if (utt > 0 || cap > 0 || snd > 0) {
        console.log(`[worker] done utt=${utt} caption=${cap} sounds=${snd}`);
      }
    } catch (err) {
      console.error(`[worker] ${err.message}`);
    } finally {
      busy = false;
    }
  }, intervalMs);
}
