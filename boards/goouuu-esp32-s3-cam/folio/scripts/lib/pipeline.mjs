import { readFileSync, unlinkSync, writeFileSync } from "node:fs";
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
  pendingFrames,
} from "./db.mjs";
import { captionFrame } from "./lm.mjs";
import { isSpeechChunk, pcmEnergy, transcribeWav } from "./whisper.mjs";
import { dayFromIso, isoNow, parseMetaHeader, writeWav } from "./util.mjs";

export function ingestAudioChunk(deviceId, pcmBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const seq = Number(meta.seq ?? 0);
  const dir = PATHS.audioDir(day);
  const path = join(dir, `${deviceId}-${seq}-${Date.now()}.pcm`);
  writeFileSync(path, pcmBuffer);

  const energy = pcmEnergy(pcmBuffer);
  const id = insertAudioChunk(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    seq,
    path,
    duration_ms: 1000,
    energy,
  });

  return { id, energy, speech: isSpeechChunk(energy) };
}

export function ingestFrame(deviceId, jpegBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const dir = PATHS.frameDir(day);
  const path = join(dir, `${deviceId}-${Date.now()}.jpg`);
  writeFileSync(path, jpegBuffer);

  const id = insertFrame(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    path,
    reason: meta.reason ?? "unknown",
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

export async function processPendingAudio(limit = 10) {
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

export async function processPendingFrames(limit = 5) {
  const db = openDb();
  const frames = pendingFrames(db, limit);
  const results = [];

  for (const frame of frames) {
    try {
      const buf = readFileSync(frame.path);
      const b64 = buf.toString("base64");
      const scene = await captionFrame(b64, frame.reason);
      const caption = `${scene.scene}: ${scene.activity}. ${scene.note || ""}`.trim();
      markFrameProcessed(db, frame.id, caption, JSON.stringify(scene));
      results.push({ id: frame.id, caption: caption.slice(0, 80) });
    } catch (err) {
      markFrameProcessed(db, frame.id, null, JSON.stringify({ error: err.message }));
      results.push({ id: frame.id, error: err.message });
    }
  }

  return results;
}

export function startProcessingLoop(intervalMs = 5000) {
  let busy = false;
  return setInterval(async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const audio = await processPendingAudio(8);
      const frames = await processPendingFrames(3);
      const activity = [...audio, ...frames].filter((r) => r.text || r.caption);
      if (activity.length) {
        console.log(`[pipeline] processed ${activity.length} items`);
      }
    } catch (err) {
      console.error(`[pipeline] ${err.message}`);
    } finally {
      busy = false;
    }
  }, intervalMs);
}
