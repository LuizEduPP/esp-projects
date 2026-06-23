import { speakersWithFingerprint } from "../db/speakers.mjs";
import { averageFingerprint, fingerprintSimilarity, pcmFingerprint } from "./fingerprint.mjs";

const MIN_MATCH_SCORE = 0.82;

export function identifySpeaker(db, pcmBuffer) {
  const probe = pcmFingerprint(pcmBuffer);
  if (!probe.length) {
    return { speaker_id: null, confidence: 0 };
  }

  let best = { speaker_id: null, confidence: 0, display_name: null };
  for (const speaker of speakersWithFingerprint(db)) {
    let profile;
    try {
      profile = JSON.parse(speaker.profile_json || "{}");
    } catch {
      continue;
    }
    const score = fingerprintSimilarity(probe, profile.fingerprint);
    if (score > best.confidence) {
      best = { speaker_id: speaker.id, confidence: score, display_name: speaker.display_name };
    }
  }

  if (best.confidence < MIN_MATCH_SCORE) {
    return { speaker_id: null, confidence: best.confidence };
  }
  return best;
}

export function enrollFingerprint(db, speakerId, pcmBuffer) {
  const fp = pcmFingerprint(pcmBuffer);
  const speaker = db.prepare("SELECT profile_json FROM speakers WHERE id = ?").get(speakerId);
  if (!speaker) {
    throw new Error(`speaker not found: ${speakerId}`);
  }
  let profile = {};
  try {
    profile = JSON.parse(speaker.profile_json || "{}");
  } catch {
    profile = {};
  }
  const samples = Array.isArray(profile.fingerprint_samples) ? profile.fingerprint_samples : [];
  if (Array.isArray(profile.fingerprint)) {
    samples.push(profile.fingerprint);
  }
  samples.push(fp);
  const trimmed = samples.slice(-8);
  profile.fingerprint = averageFingerprint(trimmed);
  profile.fingerprint_samples = trimmed;
  db.prepare("UPDATE speakers SET profile_json = ? WHERE id = ?").run(
    JSON.stringify(profile),
    speakerId,
  );
  return profile.fingerprint;
}
