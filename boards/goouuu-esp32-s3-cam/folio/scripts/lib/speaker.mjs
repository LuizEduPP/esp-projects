import { speakersWithFingerprint } from "./db/speakers.mjs";
import { CFG } from "./config/index.mjs";


function pcmSamples(pcmBuffer) {
  if (!pcmBuffer?.length) {
    return new Int16Array(0);
  }
  return new Int16Array(pcmBuffer.buffer, pcmBuffer.byteOffset, pcmBuffer.length / 2);
}

function bandEnergy(samples, start, end) {
  let sum = 0;
  const n = Math.max(1, end - start);
  for (let i = start; i < end && i < samples.length; i++) {
    const x = samples[i] / 32768;
    sum += x * x;
  }
  return Math.sqrt(sum / n);
}

export function pcmFingerprint(pcmBuffer) {
  const samples = pcmSamples(pcmBuffer);
  const n = samples.length;
  if (n === 0) {
    return [];
  }

  let sumSq = 0;
  let zc = 0;
  for (let i = 0; i < n; i++) {
    const x = samples[i] / 32768;
    sumSq += x * x;
    if (i > 0 && (samples[i] >= 0) !== (samples[i - 1] >= 0)) {
      zc++;
    }
  }
  const rms = Math.sqrt(sumSq / n);
  const zcr = zc / n;

  const third = Math.max(1, Math.floor(n / 3));
  const e0 = bandEnergy(samples, 0, third);
  const e1 = bandEnergy(samples, third, third * 2);
  const e2 = bandEnergy(samples, third * 2, n);

  const vec = [rms, zcr, e0, e1, e2, e1 / (e0 + 1e-6), e2 / (e1 + 1e-6), Math.min(1, n / 16000)];
  const norm = Math.sqrt(vec.reduce((s, v) => s + v * v, 0)) || 1;
  return vec.map((v) => v / norm);
}

export function fingerprintSimilarity(a, b) {
  if (!Array.isArray(a) || !Array.isArray(b) || a.length !== b.length || !a.length) {
    return 0;
  }
  let dot = 0;
  for (let i = 0; i < a.length; i++) {
    dot += a[i] * b[i];
  }
  return dot;
}

export function averageFingerprint(vectors) {
  if (!vectors.length) {
    return [];
  }
  const dim = vectors[0].length;
  const out = new Array(dim).fill(0);
  for (const v of vectors) {
    for (let i = 0; i < dim; i++) {
      out[i] += v[i];
    }
  }
  const norm = Math.sqrt(out.reduce((s, v) => s + v * v, 0)) || 1;
  return out.map((v) => v / vectors.length / norm);
}

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

  if (best.confidence < CFG.speakerMinMatchScore) {
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
