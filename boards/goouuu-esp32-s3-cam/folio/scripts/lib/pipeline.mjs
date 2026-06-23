import { mkdirSync, readFileSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "./config.mjs";
import {
  ensureDevice,
  insertAudioChunk,
  insertEvent,
  insertFrame,
  insertUtterance,
  markAudioProcessed,
  markFrameProcessed,
  openDb,
  pendingAudioChunks,
  pendingCounts,
  pendingFrames,
} from "./db.mjs";
import { captionFrame } from "./lm.mjs";
import { isSpeechChunk, pcmEnergy, transcribeWav } from "./whisper.mjs";
import { dayFromIso, isoNow, parseMetaHeader, writeWav } from "./util.mjs";

/** Last LM vision call — rate-limit frame captioning. */
let lastFrameLmAt = 0;

// ── Ingest (fast path: save files + DB + lightweight events; no LM) ─────────

export function ingestAudioChunk(deviceId, pcmBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const seq = Number(meta.seq ?? 0);
  const dir = PATHS.audioDir(day);
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `${deviceId}-${seq}-${Date.now()}.pcm`);
  writeFileSync(path, pcmBuffer);

  const energy = pcmEnergy(pcmBuffer);
  const speech = isSpeechChunk(energy);
  const id = insertAudioChunk(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    seq,
    path,
    duration_ms: 1000,
    energy,
  });

  if (speech) {
    insertEvent(db, {
      device_id: deviceId,
      at: capturedAt,
      kind: "presence",
      payload_json: JSON.stringify({ source: "audio", energy, seq, chunk_id: id }),
    });
  }

  return { id, energy, speech };
}

export function ingestFrame(deviceId, jpegBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const dir = PATHS.frameDir(day);
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `${deviceId}-${Date.now()}.jpg`);
  writeFileSync(path, jpegBuffer);

  const id = insertFrame(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    path,
    reason: meta.reason ?? "unknown",
  });

  insertEvent(db, {
    device_id: deviceId,
    at: capturedAt,
    kind: "frame",
    payload_json: JSON.stringify({
      frame_id: id,
      reason: meta.reason ?? "unknown",
      bytes: jpegBuffer.length,
      pending: true,
    }),
  });

  return { id, reason: meta.reason };
}

export function ingestEvent(deviceId, body) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const kind = String(body.kind ?? "unknown");
  insertEvent(db, {
    device_id: deviceId,
    at: isoNow(),
    kind,
    payload_json: JSON.stringify(body.payload ?? {}),
  });
  return { ok: true };
}

// ── Pending queue workers (Whisper + LM; not called on ingest) ───────────────

export async function processPendingAudio(limit = CFG.pipelineAudioBatch) {
  const db = openDb();
  const chunks = pendingAudioChunks(db, limit);
  const results = [];

  for (const chunk of chunks) {
    if (!isSpeechChunk(chunk.energy ?? 0)) {
      markAudioProcessed(db, chunk.id);
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
      } else {
        results.push({ id: chunk.id, skipped: "empty_stt" });
      }
    } catch (err) {
      results.push({ id: chunk.id, error: err.message });
    } finally {
      try {
        unlinkSync(wavPath);
      } catch {
        /* ignore */
      }
    }

    markAudioProcessed(db, chunk.id);
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
      console.warn(`[pipeline] frame ${frame.id} LM fail: ${err.message}`);
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
    `[pipeline] worker every ${intervalMs}ms · audio batch=${CFG.pipelineAudioBatch} · ` +
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
          "[pipeline] Whisper not available — utterances disabled. " +
            "pip install openai-whisper or set FOLIO_WHISPER_BIN",
        );
      }

      const done = [...audio, ...frames].filter((r) => r.text || r.caption);
      if (done.length) {
        console.log(
          `[pipeline] queue audio=${pending.audio} frames=${pending.frames} → ` +
            `done ${done.length} (utt=${audio.filter((r) => r.text).length} ` +
            `caption=${frames.filter((r) => r.caption).length})`,
        );
      }
    } catch (err) {
      console.error(`[pipeline] ${err.message}`);
    } finally {
      busy = false;
    }
  }, intervalMs);
}
