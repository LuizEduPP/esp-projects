import { existsSync, readFileSync, unlinkSync } from "node:fs";
import { CFG } from "./config.mjs";
import {
  deleteAudioChunk,
  insertEvent,
  insertUtterance,
  markAudioProcessed,
  markFrameProcessed,
  openDb,
  pendingAudioChunks,
  pendingCounts,
  pendingFrames,
} from "./db.mjs";
import { captionFrame } from "./lm.mjs";
import { isSpeechChunk, transcribeWav } from "./whisper.mjs";
import { writeWav } from "./util.mjs";

let lastFrameLmAt = 0;

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
    console.log(
      `[retention] removed ${files} stale audio chunks (keep transcripts ${CFG.audioRetentionDays}d)`,
    );
  }
  return { files };
}

export function startRetentionLoop() {
  runRetentionPass();
  setInterval(runRetentionPass, CFG.audioRetentionSweepMs);
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
    if (!isSpeechChunk(chunk.energy ?? 0)) {
      discardAudioChunk(db, chunk);
      results.push({ id: chunk.id, skipped: "silence" });
      continue;
    }

    const pcm = readFileSync(chunk.path);
    const wavPath = chunk.path.replace(/\.pcm$/, ".wav");
    writeWav(wavPath, pcm, CFG.audioSampleRate);

    try {
      const stt = await transcribeWav(wavPath);
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
        discardAudioChunk(db, chunk);
        results.push({ id: chunk.id, skipped: "empty_stt" });
      }
    } catch (err) {
      results.push({ id: chunk.id, error: err.message });
      console.warn(`[worker] audio ${chunk.id} whisper: ${err.message}`);
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
  let loggedWhisperMissing = false;

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

      const audio = await processPendingAudio();
      const frames = await processPendingFrames();

      const whisperErrors = audio.filter((r) => r.error?.includes("Whisper"));
      if (whisperErrors.length && !loggedWhisperMissing) {
        loggedWhisperMissing = true;
        console.warn(
          "[worker] Whisper not available — utterances disabled. " +
            "pip install openai-whisper or set FOLIO_WHISPER_BIN",
        );
      }

      const done = [...audio, ...frames].filter((r) => r.text || r.caption);
      const whisperOk = audio.filter((r) => r.text);
      const whisperErr = audio.filter((r) => r.error);
      if (whisperOk.length) {
        console.log(`[worker] whisper ok=${whisperOk.length} "${whisperOk[0].text}"`);
      }
      if (whisperErr.length) {
        console.warn(`[worker] whisper fail=${whisperErr.length} ${whisperErr[0].error}`);
      }
      if (done.length) {
        console.log(
          `[worker] queue audio=${pending.audio} frames=${pending.frames} → ` +
            `done ${done.length} (utt=${audio.filter((r) => r.text).length} ` +
            `caption=${frames.filter((r) => r.caption).length})`,
        );
      }
    } catch (err) {
      console.error(`[worker] ${err.message}`);
    } finally {
      busy = false;
    }
  }, intervalMs);
}
