/**
 * Adaptive gates from ingested signal — no fixed thresholds in config.
 */

function quantile(sorted, p) {
  if (!sorted.length) {
    return 0;
  }
  const idx = Math.min(sorted.length - 1, Math.floor(p * sorted.length));
  return sorted[idx];
}

/** Speech / ambient gates from recent chunk RMS energies. */
export function calibrateAudioGates(energies) {
  const vals = energies.filter((e) => Number.isFinite(e) && e > 0);
  if (vals.length < 12) {
    return null;
  }
  const sorted = [...vals].sort((a, b) => a - b);
  const floor = quantile(sorted, 0.55);
  const spread = Math.max(quantile(sorted, 0.92) - floor, 1e-5);
  return {
    speechEnergyThreshold: floor + spread * 0.42,
    soundMinEnergy: floor + spread * 0.14,
  };
}

/** Motion gate from recent frame motion scores (0..1). */
export function calibrateMotionGate(scores) {
  const vals = scores.filter((s) => Number.isFinite(s) && s >= 0);
  if (vals.length < 8) {
    return null;
  }
  const sorted = [...vals].sort((a, b) => a - b);
  const floor = quantile(sorted, 0.5);
  const spread = Math.max(sorted[sorted.length - 1] - floor, 0.008);
  return { motionMin: floor + spread * 0.28 };
}

export function loadCalibrationSamples(db) {
  const energies = db
    .prepare(
      `SELECT energy FROM audio_chunks
       WHERE energy IS NOT NULL AND captured_at >= datetime('now', '-18 hours')
       ORDER BY captured_at DESC LIMIT 500`,
    )
    .all()
    .map((r) => r.energy);

  const motions = db
    .prepare(
      `SELECT json_extract(scene_json, '$.motion.score') AS score FROM frames
       WHERE scene_json IS NOT NULL AND captured_at >= datetime('now', '-24 hours')
       ORDER BY captured_at DESC LIMIT 300`,
    )
    .all()
    .map((r) => Number(r.score))
    .filter((n) => Number.isFinite(n));

  return { energies, motions };
}

export function applyCalibrationToCfg(CFG, db) {
  const { energies, motions } = loadCalibrationSamples(db);
  const audio = calibrateAudioGates(energies);
  const motion = calibrateMotionGate(motions);

  if (audio) {
    CFG.speechEnergyThreshold = audio.speechEnergyThreshold;
    CFG.perceptionSoundMinEnergy = audio.soundMinEnergy;
  } else {
    CFG.speechEnergyThreshold = 0;
    CFG.perceptionSoundMinEnergy = 0;
  }

  if (motion) {
    CFG.perceptionMotionMin = Math.max(0.04, motion.motionMin);
  } else {
    CFG.perceptionMotionMin = 0.04;
  }

  return {
    calibrated: Boolean(audio || motion),
    audio,
    motion,
    samples: { energies: energies.length, motions: motions.length },
  };
}
