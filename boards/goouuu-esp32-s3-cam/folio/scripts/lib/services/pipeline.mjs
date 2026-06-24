import { existsSync, unlinkSync } from "node:fs";
import { adaptivePipelineIntervalMs } from "../bootstrap.mjs";
import { applyCalibrationToCfg } from "../calibration.mjs";
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
  if (pendingFrameCount >= 16) {
    return 2500;
  }
  if (pendingFrameCount >= 6) {
    return 6000;
  }
  const base = CFG.frameCaptionIntervalMs ?? 60_000;
  if (pendingFrameCount <= 1) {
    return base;
  }
  return Math.max(4000, Math.floor(base / 8));
}

function maxCaptionsPerCycle(pendingFrameCount) {
  if (pendingFrameCount >= 20) {
    return 6;
  }
  if (pendingFrameCount >= 8) {
    return 4;
  }
  return Math.min(3, Math.max(1, Math.ceil(Math.sqrt(pendingFrameCount))));
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
  const maxLm = bypassGap ? limit : maxCaptionsPerCycle(pending.frames);
  const frames = pendingFrames(db, limit);
  const results = [];
  let lmCount = 0;

  for (const frame of frames) {
    const lmBlocked = !bypassGap && minGap > 0 && Date.now() - lastFrameLmAt < minGap;
    if (lmBlocked && lmCount >= maxLm) {
      break;
    }
    const allowLm = bypassGap || !lmBlocked || lmCount < maxLm;
    try {
      const result = await processFrame(db, frame, { allowLm });
      if (result.deferred) {
        break;
      }
      results.push(result);
      if (result.usedLm) {
        lastFrameLmAt = Date.now();
        lmCount++;
        console.log(`[perception] frame ${frame.id}: ${result.caption?.slice(0, 80) ?? "caption"}`);
      } else if (result.skipped) {
        console.log(`[perception] frame ${frame.id}: skip ${result.skipped}`);
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
  let timer = null;

  console.log(`[worker] adaptive · base interval ${intervalMs}ms`);

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
      let stagnantRounds = 0;
      for (let round = 0; round < 24; round++) {
        if (after.audio > 0) {
          console.log(`[worker] audio queue ${after.audio}`);
        }
        if (after.frames > 0) {
          console.log(`[worker] frame queue ${after.frames}`);
        }

        const audioResults = await processPendingAudio();
        const frameResults = await processPendingFrames(CFG.pipelineFrameBatch, {
          bypassGap: after.frames >= 8,
        });
        const next = pendingCounts(db);
        const madeProgress =
          audioResults.length > 0 ||
          frameResults.length > 0 ||
          next.frames < after.frames ||
          next.audio < after.audio;

        after = next;
        if (after.audio === 0 && after.frames === 0) {
          break;
        }
        if (!madeProgress) {
          stagnantRounds++;
          if (stagnantRounds >= 2) {
            break;
          }
        } else {
          stagnantRounds = 0;
        }
      }

      await maybeAutoInsights(before, after);
      applyCalibrationToCfg(CFG, db);
    } catch (err) {
      console.error(`[worker] ${err.message}`);
    } finally {
      busy = false;
      const pending = pendingCounts(openDb());
      const nextMs = adaptivePipelineIntervalMs(pending.audio + pending.frames);
      clearTimeout(timer);
      timer = setTimeout(tick, nextMs);
    }
  };

  tick();
}
