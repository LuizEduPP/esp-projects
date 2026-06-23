#!/usr/bin/env node
/**
 * folio-digest — run multi-pass digest (A→D) for a day.
 * Usage: node folio-digest.mjs [--today] [--day YYYY-MM-DD]
 */
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "./lib/config.mjs";
import { getDigest, openDb } from "./lib/db.mjs";
import { runDigestPipeline } from "./lib/passes.mjs";
import { errMsg } from "./lib/util.mjs";

function parseArgs(argv) {
  let day = null;
  for (let i = 2; i < argv.length; i++) {
    if (argv[i] === "--today") {
      day = new Date().toISOString().slice(0, 10);
    } else if (argv[i] === "--day" && argv[i + 1]) {
      day = argv[++i];
    }
  }
  if (!day) {
    day = new Date().toISOString().slice(0, 10);
  }
  return day;
}

async function main() {
  const day = parseArgs(process.argv);
  console.log(`[folio-digest] day=${day} data=${CFG.dataDir}`);

  const db = openDb();
  const result = await runDigestPipeline(db, day);
  const row = getDigest(db, day);

  const mdPath = join(PATHS.digestDir(), `${day}.md`);
  writeFileSync(mdPath, result.prose, "utf8");

  console.log(`\n${"─".repeat(60)}\n`);
  console.log(result.prose);
  console.log(`\n${"─".repeat(60)}`);
  console.log(`saved: ${mdPath}`);
  console.log(`passes stored in folio.db digests.id=${row?.id ?? "?"}`);
}

main().catch((err) => {
  console.error(`[folio-digest] ${errMsg(err)}`);
  process.exit(1);
});
