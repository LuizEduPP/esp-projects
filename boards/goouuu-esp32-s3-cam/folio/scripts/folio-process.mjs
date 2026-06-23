#!/usr/bin/env node
/** Drain pending audio (Whisper) and frames (LM) once — for manual/cron runs. */
import { CFG } from "./lib/config.mjs";
import { openDb, pendingCounts } from "./lib/db.mjs";
import { runPendingQueueOnce } from "./lib/pipeline.mjs";

const db = openDb();
const before = pendingCounts(db);
console.log(`[process] pending audio=${before.audio} frames=${before.frames}`);

const { audio, frames } = await runPendingQueueOnce();

const after = pendingCounts(db);
console.log(
  `[process] done utterances=${audio.filter((r) => r.text).length} ` +
    `captions=${frames.filter((r) => r.caption).length} ` +
    `remaining audio=${after.audio} frames=${after.frames}`,
);

if (CFG.frameCaptionIntervalMs > 0 && before.frames > 0 && frames.length === 0) {
  console.log(`[process] LM rate limit (${CFG.frameCaptionIntervalMs}ms) — try again later`);
}
