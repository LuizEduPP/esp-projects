#!/usr/bin/env node
/**
 * folio-digest — manual digest run (brain does this automatically).
 * Usage: node folio-digest.mjs [--today] [--day YYYY-MM-DD] [--force]
 */
import { CFG } from "./lib/config.mjs";
import { getDigest, openDb } from "./lib/db.mjs";
import { runDigestForDay } from "./lib/digest_scheduler.mjs";
import { errMsg } from "./lib/util.mjs";

function parseArgs(argv) {
  let day = null;
  let force = false;
  for (let i = 2; i < argv.length; i++) {
    if (argv[i] === "--today") {
      day = new Date().toISOString().slice(0, 10);
    } else if (argv[i] === "--day" && argv[i + 1]) {
      day = argv[++i];
    } else if (argv[i] === "--force") {
      force = true;
    }
  }
  if (!day) {
    day = new Date().toISOString().slice(0, 10);
  }
  return { day, force };
}

async function main() {
  const { day, force } = parseArgs(process.argv);
  console.log(`[folio-digest] day=${day} data=${CFG.dataDir} force=${force}`);

  const db = openDb();
  const result = await runDigestForDay(db, day, { force });
  if (result.skipped) {
    console.log(`[folio-digest] skipped (${result.reason})`);
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

main().catch((err) => {
  console.error(`[folio-digest] ${errMsg(err)}`);
  process.exit(1);
});
