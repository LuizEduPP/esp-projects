import { readFileSync, unlinkSync } from "node:fs";
import { CFG } from "./config.mjs";
import {
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
import { discardAudioChunk } from "./retention.mjs";
import { isSpeechChunk, transcribeWav } from "./whisper.mjs";
import { writeWav } from "./util.mjs";

let lastFrameLmAt = 0;

export async function processPendingAudio(limit = CFG.pipelineAudioBatch) {
  const db = openDb();
  const pending = pendingCounts(db);
  if (pending.audio > 200) {
    limit = Math.min(12, limit * 3);
  } else if (pending.audio > 50) {
    limit = Math.min(8, limit * 2);
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
    writeWav(wavPath, pcm);

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
