import { existsSync, readFileSync, unlinkSync } from "node:fs";
import { CFG } from "./config.mjs";
import {
  bumpSttAttempts,
  deleteAudioChunk,
  insertEvent,
  insertUtterance,
  markAudioProcessed,
  markFrameProcessed,
  openDb,
  pendingAudioChunks,
  pendingCounts,
  pendingFrames,
  pruneExpiredPcm,
} from "./db.mjs";
import { captionFrame } from "./lm.mjs";
import { isSpeechChunk, transcribeWav } from "./whisper.mjs";
import { writeWav } from "./util.mjs";

const MAX_STT_ATTEMPTS = 3;
let lastFrameLmAt = 0;
let loggedWhisperMissing = false;

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
       WHERE c.processed = 1 AND u.id IS NULL AND c.path != ''`,
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
    if (!chunk.path || !existsSync(chunk.path)) {
      discardAudioChunk(db, chunk);
      results.push({ id: chunk.id, skipped: "missing_file" });
      continue;
    }

    if (!isSpeechChunk(chunk.energy ?? 0)) {
      console.log(
        `[whisper] chunk=${chunk.id} skip silence energy=${(chunk.energy ?? 0).toFixed(4)}`,
      );
      discardAudioChunk(db, chunk);
      results.push({ id: chunk.id, skipped: "silence" });
      continue;
    }

    const pcm = readFileSync(chunk.path);
    const wavPath = chunk.path.replace(/\.pcm$/, ".wav");
    writeWav(wavPath, pcm, CFG.audioSampleRate);

    try {
      const stt = await transcribeWav(wavPath, { chunkId: chunk.id });
      if (stt.text) {
        insertUtterance(db, {
          chunk_id: chunk.id,
          speaker_id: null,
          started_at: chunk.captured_at,
          ended_at: chunk.captured_at,
          text: stt.text,
          confidence: stt.confidence,
        });
        results.push({ id: chunk.id, text: stt.text.slice(0, 80) });
        markAudioProcessed(db, chunk.id);
      } else {
        console.log(`[whisper] chunk=${chunk.id} discard — STT returned no text`);
        discardAudioChunk(db, chunk);
        results.push({ id: chunk.id, skipped: "empty_stt" });
      }
    } catch (err) {
      const attempts = bumpSttAttempts(db, chunk.id);
      if (attempts >= MAX_STT_ATTEMPTS) {
        console.error(`[whisper] chunk=${chunk.id} discard after ${attempts} failures`);
        discardAudioChunk(db, chunk);
        results.push({ id: chunk.id, skipped: "stt_failed", error: err.message });
      } else {
        results.push({ id: chunk.id, error: err.message, retry: attempts });
      }
    } finally {
      try {
        unlinkSync(wavPath);
      } catch {
        /* ignore */
      }
    }
  }

  return results;
}

export async function processPendingFrames(limit = CFG.pipelineFrameBatch) {
  const minGap = CFG.frameCaptionIntervalMs;
  if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
    return [];
  }

  const db = openDb();
  const frames = pendingFrames(db, limit);
  const results = [];

  for (const frame of frames) {
    try {
      const buf = readFileSync(frame.path);
      const b64 = buf.toString("base64");
      lastFrameLmAt = Date.now();
      const scene = await captionFrame(b64, frame.reason);
      const caption = `${scene.scene}: ${scene.activity}. ${scene.note || ""}`.trim();
      markFrameProcessed(db, frame.id, caption, JSON.stringify(scene));
      if (scene.person_present === true || Number(scene.people) > 0) {
        insertEvent(db, {
          device_id: frame.device_id,
          at: frame.captured_at,
          kind: "presence",
          payload_json: JSON.stringify({
            source: "camera",
            people: Number(scene.people) || 0,
            frame_id: frame.id,
          }),
        });
      }
      results.push({ id: frame.id, caption: caption.slice(0, 80) });
    } catch (err) {
      results.push({ id: frame.id, error: err.message });
      console.warn(`[worker] frame ${frame.id} LM fail: ${err.message}`);
    }
  }

  return results;
}

export async function runPendingQueueOnce() {
  const audio = await processPendingAudio();
  const frames = await processPendingFrames();
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
      if (utt > 0 || cap > 0) {
        console.log(`[worker] done utt=${utt} caption=${cap}`);
      }
    } catch (err) {
      console.error(`[worker] ${err.message}`);
    } finally {
      busy = false;
    }
  }, intervalMs);
}
