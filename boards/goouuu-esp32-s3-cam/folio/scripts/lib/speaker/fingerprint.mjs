/** Lightweight PCM fingerprint for speaker matching (no ML deps). */

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
