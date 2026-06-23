import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "./config.mjs";
import {
  ensureDevice,
  insertAudioChunk,
  insertEvent,
  insertFrame,
  openDb,
} from "./db.mjs";
import { isSpeechChunk, shouldStoreAudioChunk } from "./whisper.mjs";
import { dayFromIso, isoNow, parseMetaHeader } from "./util.mjs";

export function ingestAudioChunk(deviceId, pcmBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const seq = Number(meta.seq ?? 0);
  const gate = shouldStoreAudioChunk(pcmBuffer);

  if (!gate.store) {
    return { id: null, energy: gate.energy, speech: false, skipped: gate.reason };
  }

  const dir = PATHS.audioDir(day);
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `${deviceId}-${seq}-${Date.now()}.pcm`);
  writeFileSync(path, pcmBuffer);

  const id = insertAudioChunk(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    seq,
    path,
    duration_ms: CFG.audioChunkMs,
    energy: gate.energy,
  });

  insertEvent(db, {
    device_id: deviceId,
    at: capturedAt,
    kind: "presence",
    payload_json: JSON.stringify({ source: "audio", energy: gate.energy, seq, chunk_id: id }),
  });

  return { id, energy: gate.energy, speech: true };
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
