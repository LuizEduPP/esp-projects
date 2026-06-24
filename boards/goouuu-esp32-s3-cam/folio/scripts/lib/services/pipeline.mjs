import { existsSync, unlinkSync } from "node:fs";
import { CFG } from "../config.mjs";
import { deleteAudioChunk, openDb, pendingAudioChunks, pendingCounts, pendingFrames, pruneExpiredPcm } from "../db/index.mjs";
import { processAudioChunk, processFrame } from "../perception/index.mjs";
import { needsInsightsRefresh, runDayInsights } from "./insights.mjs";
import { today } from "../util.mjs";

let lastFrameLmAt = 0;
let lastInsightDrainAt = 0;

function adaptiveBatch(pending, base, max) {
  if (pending <= 0) {
    return base;
  }
  return Math.min(max, Math.max(base, Math.ceil(Math.sqrt(pending) * 3)));
}

/** Shorter gap when frames pile up — scales with queue depth. */
function frameCaptionGapMs(pendingFrameCount) {
  const base = CFG.frameCaptionIntervalMs;
  if (pendingFrameCount <= 1) {
    return base;
  }
  const factor = Math.min(1, 12 / Math.sqrt(pendingFrameCount));
  return Math.max(1500, Math.round(base * factor));
}

function maxCaptionsPerCycle(pendingFrameCount) {
  return Math.min(6, Math.max(1, Math.ceil(Math.sqrt(pendingFrameCount) / 4)));
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

function isSttError(err) {
  const msg = String(err?.message ?? err ?? "");
  return /whisper|STT|transcriptions/i.test(msg);
}

async function maybeAutoInsights(before, after) {
  if (!CFG.insightsAuto) {
    return;
  }
  const was = before.audio + before.frames;
  const now = after.audio + after.frames;
  if (was < 8 || now > was * 0.25) {
    return;
  }
  if (Date.now() - lastInsightDrainAt < 120_000) {
    return;
  }
  const db = openDb();
  const day = today();
  const check = needsInsightsRefresh(db, day);
  if (!check.needed) {
    return;
  }
  lastInsightDrainAt = Date.now();
  try {
    await runDayInsights(db, day);
  } catch (err) {
    console.error(`[insights] auto after drain: ${err.message}`);
  }
}

export async function processPendingAudio(limit = CFG.pipelineAudioBatch) {
  const db = openDb();
  const pending = pendingCounts(db);
  limit = adaptiveBatch(pending.audio, limit, CFG.workerBatchMaxHigh ?? 16);

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
  limit = adaptiveBatch(pending.frames, limit, CFG.workerFrameSkipBatch ?? 48);

  const minGap = bypassGap ? 0 : frameCaptionGapMs(pending.frames);
  if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
    return [];
  }

  const maxLm = bypassGap ? limit : maxCaptionsPerCycle(pending.frames);
  const frames = pendingFrames(db, limit);
  const results = [];
  let lmCount = 0;

  for (const frame of frames) {
    if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
      break;
    }
    if (minGap > 0 && lmCount >= maxLm) {
      break;
    }
    try {
      const result = await processFrame(db, frame);
      results.push(result);
      if (result.usedLm) {
        lastFrameLmAt = Date.now();
        lmCount++;
      }
    } catch (err) {
      results.push({ id: frame.id, error: err.message, usedLm: false });
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
      `frame batch=${CFG.pipelineFrameBatch}`,
  );

  const tick = async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const db = openDb();
      const before = pendingCounts(db);
      if (before.audio === 0 && before.frames === 0) {
        return;
      }

      let after = before;
      for (let round = 0; round < 24; round++) {
        if (after.audio > 0) {
          console.log(`[worker] audio queue ${after.audio}`);
        }
        if (after.frames > 0) {
          console.log(`[worker] frame queue ${after.frames}`);
        }

        await processPendingAudio();
        await processPendingFrames();
        const next = pendingCounts(db);
        if (next.audio + next.frames >= after.audio + after.frames) {
          after = next;
          break;
        }
        after = next;
        if (after.audio === 0 && after.frames === 0) {
          break;
        }
      }

      await maybeAutoInsights(before, after);
    } catch (err) {
      console.error(`[worker] ${err.message}`);
    } finally {
      busy = false;
    }
  };

  setInterval(tick, intervalMs);
  tick();
}
