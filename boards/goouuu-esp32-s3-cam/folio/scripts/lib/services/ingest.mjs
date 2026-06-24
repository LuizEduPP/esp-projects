import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "../config.mjs";
import { ensureDevice, insertAudioChunk, insertEvent, insertFrame, openDb } from "../db/index.mjs";
import { isSpeechChunk, pcmEnergy, shouldStoreAudioChunk } from "../stt.mjs";
import { dayFromIso, isoNow, parseMetaHeader } from "../util.mjs";

export function ingestAudioChunk(deviceId, pcmBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const seq = Number(meta.seq ?? 0);
  const deviceMs = meta.ts_ms != null ? Number(meta.ts_ms) : null;

  const metaEnergy = meta.energy != null ? Number(meta.energy) : null;
  const energy = Number.isFinite(metaEnergy) ? metaEnergy : pcmEnergy(pcmBuffer);
  const gate = shouldStoreAudioChunk(pcmBuffer, energy);

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
    energy,
    device_ms: deviceMs,
  });

  if (isSpeechChunk(energy)) {
    insertEvent(db, {
      device_id: deviceId,
      at: capturedAt,
      kind: "presence",
      payload_json: JSON.stringify({
        source: "audio",
        energy,
        seq,
        chunk_id: id,
        device_ms: deviceMs,
      }),
    });
  }

  return { id, energy, speech: isSpeechChunk(energy), device_ms: deviceMs };
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
      device_ms: meta.ts_ms != null ? Number(meta.ts_ms) : null,
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
