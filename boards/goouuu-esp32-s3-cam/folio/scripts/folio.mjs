#!/usr/bin/env node
/**
 * folio — CLI for brain-side tasks.
 * Usage: folio.mjs <process|enroll> [options]
 */
import { openDb, pendingCounts, upsertEntity, upsertSpeaker } from "./lib/db/index.mjs";
import { enrollFingerprint } from "./lib/speaker/identify.mjs";
import { runDayInsights } from "./lib/services/insights/index.mjs";
import { runPendingQueueOnce } from "./lib/services/index.mjs";
import { reindexAllMemories } from "./lib/memory/index.mjs";
import { errMsg } from "./lib/util/index.mjs";
import { readFileSync } from "node:fs";

function usage() {
  console.error(`Usage:
  folio.mjs process
  folio.mjs insights [--today] [--day YYYY-MM-DD] [--force]
  folio.mjs memory reindex
  folio.mjs enroll <speaker_id> <display_name> [--pcm path/to/sample.pcm]`);
  process.exit(1);
}

async function cmdProcess() {
  const db = openDb();
  const start = pendingCounts(db);
  console.log(`[process] pending audio=${start.audio} frames=${start.frames}`);

  let totalUtt = 0;
  let totalCap = 0;
  let sttFail = 0;
  let rounds = 0;
  const maxRounds = Math.max(50, start.frames + start.audio + 5);

  while (rounds++ < maxRounds) {
    const before = pendingCounts(db);
    if (before.audio === 0 && before.frames === 0) {
      break;
    }

    const { audio, frames } = await runPendingQueueOnce({ bypassFrameGap: true });
    totalUtt += audio.filter((r) => r.text).length;
    totalCap += frames.filter((r) => r.caption).length;
    sttFail += audio.filter((r) => r.skipped === "stt_failed" || r.error).length;

    const after = pendingCounts(db);
    if (after.frames === before.frames && after.audio === before.audio) {
      break;
    }
  }

  const end = pendingCounts(db);

  console.log(
    `[process] done utterances=${totalUtt} captions=${totalCap} ` +
      `remaining audio=${end.audio} frames=${end.frames}` +
      (sttFail ? ` stt_issues=${sttFail}` : ""),
  );
}

function cmdEnroll(argv) {
  let speakerId = null;
  let displayName = null;
  let pcmPath = null;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--pcm" && argv[i + 1]) {
      pcmPath = argv[++i];
    } else if (!speakerId) {
      speakerId = argv[i];
    } else if (!displayName) {
      displayName = argv[i];
    }
  }
  if (!speakerId || !displayName) {
    usage();
  }

  const db = openDb();
  upsertSpeaker(db, speakerId, displayName);
  upsertEntity(db, {
    id: `speaker:${speakerId}`,
    kind: "person",
    display_name: displayName,
    speaker_id: speakerId,
    profile_json: JSON.stringify({ enrolled: true }),
    patterns_json: JSON.stringify({}),
  });

  if (pcmPath) {
    const pcm = readFileSync(pcmPath);
    enrollFingerprint(db, speakerId, pcm);
    console.log(`Fingerprint enrolled from ${pcmPath}`);
  } else {
    console.log("Tip: pass --pcm sample.pcm to train voice fingerprint");
  }

  console.log(`Speaker enrolled: ${speakerId} (${displayName})`);
}

async function cmdInsights(argv) {
  let day = new Date().toISOString().slice(0, 10);
  let force = false;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--today") {
      day = new Date().toISOString().slice(0, 10);
    } else if (argv[i] === "--day" && argv[i + 1]) {
      day = argv[++i];
    } else if (argv[i] === "--force") {
      force = true;
    }
  }
  const db = openDb();
  const result = await runDayInsights(db, day, { force });
  if (result.skipped) {
    console.log(`[insights] skipped (${result.reason})`);
    return;
  }
  console.log(JSON.stringify(result.insights, null, 2));
}

async function cmdMemory(argv) {
  const sub = argv[0];
  if (sub === "reindex") {
    const db = openDb();
    const result = await reindexAllMemories(db);
    console.log(`[memory] reindexed ${result.days} days · ${result.chunks} chunks`);
    return;
  }
  usage();
}

async function main() {
  const [cmd, ...rest] = process.argv.slice(2);
  switch (cmd) {
    case "process":
      await cmdProcess();
      break;
    case "insights":
      await cmdInsights(rest);
      break;
    case "memory":
      await cmdMemory(rest);
      break;
    case "enroll":
      cmdEnroll(rest);
      break;
    default:
      usage();
  }
}

main().catch((err) => {
  console.error(`[folio] ${errMsg(err)}`);
  process.exit(1);
});
