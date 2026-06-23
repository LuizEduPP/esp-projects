import { existsSync, readFileSync, unlinkSync } from "node:fs";
import { CFG } from "../config/index.mjs";
import {
  bumpSttAttempts,
  deleteAudioChunk,
  insertEvent,
  insertUtterance,
  markAudioProcessed,
  updateAudioClassification,
} from "../db/index.mjs";
import { identifySpeaker } from "../speaker.mjs";
import { isSpeechChunk, transcribeWav } from "../stt.mjs";
import { writeWav } from "../util.mjs";
import { classifySound, isInterestingSound, speechLabel } from "./sound.mjs";
import { SoundKind } from "./types.mjs";

function deleteChunkFile(path) {
  if (!path) {
    return;
  }
  try {
    unlinkSync(path);
  } catch {
    /* ignore */
  }
}

function discardAudioChunk(db, chunk) {
  deleteChunkFile(chunk.path);
  deleteAudioChunk(db, chunk.id);
}

export async function processAudioChunk(db, chunk) {
  if (!chunk.path || !existsSync(chunk.path)) {
    discardAudioChunk(db, chunk);
    return { id: chunk.id, skipped: "missing_file" };
  }

  const pcm = readFileSync(chunk.path);
  const energy = chunk.energy ?? 0;

  if (!isSpeechChunk(energy)) {
    if (!CFG.perceptionStoreSounds) {
      discardAudioChunk(db, chunk);
      return { id: chunk.id, skipped: "quiet" };
    }

    const sound = await classifySound(pcm, energy, chunk.duration_ms);
    if (!isInterestingSound(sound)) {
      discardAudioChunk(db, chunk);
      return { id: chunk.id, skipped: "uninteresting" };
    }

    updateAudioClassification(db, chunk.id, {
      sound_kind: sound.kind,
      sound_label: sound.label,
      speaker_id: null,
      speaker_confidence: null,
    });
    markAudioProcessed(db, chunk.id);
    deleteChunkFile(chunk.path);

    insertEvent(db, {
      device_id: chunk.device_id,
      at: chunk.captured_at,
      kind: "sound",
      payload_json: JSON.stringify({
        kind: sound.kind,
        label: sound.label,
        confidence: sound.confidence,
        energy,
        chunk_id: chunk.id,
      }),
    });

    return { id: chunk.id, sound: sound.kind, label: sound.label };
  }

  const wavPath = chunk.path.replace(/\.pcm$/, ".wav");
  writeWav(wavPath, pcm, CFG.audioSampleRate);

  try {
    const speaker = identifySpeaker(db, pcm);
    const stt = await transcribeWav(wavPath, { chunkId: chunk.id });
    if (stt.text) {
      insertUtterance(db, {
        chunk_id: chunk.id,
        speaker_id: speaker.speaker_id,
        started_at: chunk.captured_at,
        ended_at: chunk.captured_at,
        text: stt.text,
        confidence: stt.confidence,
      });
      updateAudioClassification(db, chunk.id, {
        sound_kind: SoundKind.SPEECH,
        sound_label: speechLabel(),
        speaker_id: speaker.speaker_id,
        speaker_confidence: speaker.confidence,
      });
      markAudioProcessed(db, chunk.id);
      return {
        id: chunk.id,
        text: stt.text.slice(0, 80),
        speaker: speaker.speaker_id,
      };
    }

    if (CFG.perceptionStoreSounds) {
      const sound = await classifySound(pcm, energy, chunk.duration_ms);
      if (isInterestingSound(sound)) {
        updateAudioClassification(db, chunk.id, {
          sound_kind: sound.kind,
          sound_label: sound.label,
          speaker_id: null,
          speaker_confidence: null,
        });
        markAudioProcessed(db, chunk.id);
        deleteChunkFile(chunk.path);
        return { id: chunk.id, sound: sound.kind, skipped: "empty_stt" };
      }
    }

    discardAudioChunk(db, chunk);
    return { id: chunk.id, skipped: "empty_stt" };
  } catch (err) {
    const attempts = bumpSttAttempts(db, chunk.id);
    if (attempts >= CFG.audioSttMaxAttempts) {
      discardAudioChunk(db, chunk);
      return { id: chunk.id, skipped: "stt_failed", error: err.message };
    }
    return { id: chunk.id, error: err.message, retry: attempts };
  } finally {
    try {
      unlinkSync(wavPath);
    } catch {
      /* ignore */
    }
  }
}
