import { CFG } from "../config/index.mjs";
import { dayBounds } from "../util.mjs";
import { audioChunksForDay, utterancesForDay } from "./utterances.mjs";
import { framesForDay } from "./frames.mjs";

export function witnessStats(db, day) {
  const { start, end } = dayBounds(day);
  const speech = db
    .prepare(
      `SELECT COUNT(*) AS n FROM audio_chunks
       WHERE captured_at >= ? AND captured_at < ? AND energy >= ?`,
    )
    .get(start, end, CFG.speechEnergyThreshold).n;
  const frames = db
    .prepare(`SELECT COUNT(*) AS n FROM frames WHERE captured_at >= ? AND captured_at < ?`)
    .get(start, end).n;
  const utterances = db
    .prepare(`SELECT COUNT(*) AS n FROM utterances WHERE started_at >= ? AND started_at < ?`)
    .get(start, end).n;
  return { speech, frames, utterances };
}

export function latestWitnessAt(db, day) {
  const { start, end } = dayBounds(day);
  const row = db
    .prepare(
      `SELECT MAX(t) AS m FROM (
         SELECT MAX(captured_at) AS t FROM audio_chunks WHERE captured_at >= ? AND captured_at < ?
         UNION ALL
         SELECT MAX(captured_at) FROM frames WHERE captured_at >= ? AND captured_at < ?
         UNION ALL
         SELECT MAX(started_at) FROM utterances WHERE started_at >= ? AND started_at < ?
       )`,
    )
    .get(start, end, start, end, start, end);
  return row?.m ?? null;
}

export function alignedMomentsForDay(db, day) {
  const utterances = utterancesForDay(db, day).map((u) => ({
    id: `utt:${u.id}`,
    at: u.started_at,
    text: u.text,
  }));
  const frames = framesForDay(db, day).map((f) => ({
    id: `frm:${f.id}`,
    at: f.captured_at,
    visual: f.caption || f.scene_json,
  }));
  return [...utterances, ...frames].sort((a, b) => a.at.localeCompare(b.at));
}

export function timelineForDay(db, day) {
  const audio = audioChunksForDay(db, day).map((c) => ({
    type: "audio",
    at: c.captured_at,
    id: `aud:${c.id}`,
    chunk_id: c.id,
    energy: c.energy,
    speech: (c.energy ?? 0) >= CFG.speechEnergyThreshold,
    sound_kind: c.sound_kind ?? null,
    sound_label: c.sound_label ?? null,
    speaker_id: c.speaker_id ?? null,
    speaker_confidence: c.speaker_confidence ?? null,
    text: c.utterance_text ?? null,
    utterance_id: c.utterance_id ?? null,
    processed: !!c.processed,
    has_pcm: Boolean(c.path),
    device_ms: c.device_ms ?? null,
  }));
  const frames = framesForDay(db, day).map((f) => ({
    type: "frame",
    at: f.captured_at,
    id: `frm:${f.id}`,
    frame_id: f.id,
    caption: f.caption,
    reason: f.reason,
    processed: !!f.processed,
  }));
  return [...audio, ...frames].sort((a, b) => a.at.localeCompare(b.at));
}
