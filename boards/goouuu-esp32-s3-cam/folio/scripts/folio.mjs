#!/usr/bin/env node
/**
 * folio — CLI for brain-side tasks.
 * Usage: folio.mjs <digest|process|enroll> [options]
 */
import { CFG } from "./lib/config.mjs";
import { getDigest, openDb, memoryChunkCount, pendingCounts, upsertSpeaker } from "./lib/db.mjs";
import { runDigestForDay } from "./lib/digest/scheduler.mjs";
import { reindexMemoriesFromDigests } from "./lib/memory/index.mjs";
import { runPendingQueueOnce } from "./lib/worker.mjs";
import { errMsg, today } from "./lib/util.mjs";

function usage() {
  console.error(`Usage:
  folio.mjs digest [--today] [--day YYYY-MM-DD] [--force]
  folio.mjs process
  folio.mjs memory reindex
  folio.mjs enroll <speaker_id> <display_name>`);
  process.exit(1);
}

function parseDigestArgs(argv) {
  let day = null;
  let force = false;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === "--today") {
      day = today();
    } else if (argv[i] === "--day" && argv[i + 1]) {
      day = argv[++i];
    } else if (argv[i] === "--force") {
      force = true;
    }
  }
  return { day: day ?? today(), force };
}

async function cmdDigest(argv) {
  const { day, force } = parseDigestArgs(argv);
  console.log(`[digest] day=${day} data=${CFG.dataDir} force=${force}`);

  const db = openDb();
  const result = await runDigestForDay(db, day, { force });
  if (result.skipped) {
    console.log(`[digest] skipped (${result.reason})`);
    const row = getDigest(db, day);
    if (row?.prose) {
      console.log(row.prose);
    }
    return;
  }

  console.log(`\n${"─".repeat(60)}\n`);
  console.log(result.prose);
  console.log(`\n${"─".repeat(60)}`);
  console.log(`saved: ${result.path}`);
}

async function cmdProcess() {
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
}

function cmdEnroll(argv) {
  const speakerId = argv[0];
  const displayName = argv[1];
  if (!speakerId || !displayName) {
    usage();
  }

  upsertSpeaker(openDb(), speakerId, displayName);
  console.log(`Speaker enrolled: ${speakerId} (${displayName})`);
}

async function cmdMemory(argv) {
  const sub = argv[0];
  if (sub === "reindex") {
    const db = openDb();
    const before = memoryChunkCount(db);
    const result = await reindexMemoriesFromDigests(db);
    const after = memoryChunkCount(db);
    console.log(
      `[memory] reindexed ${result.days} days · ${result.chunks} chunks (total ${before} → ${after})`,
    );
    return;
  }
  usage();
}

async function main() {
  const [cmd, ...rest] = process.argv.slice(2);
  switch (cmd) {
    case "digest":
      await cmdDigest(rest);
      break;
    case "process":
      await cmdProcess();
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
