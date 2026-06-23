import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "../config.mjs";
import { getDigest, latestWitnessAt, openDb, witnessStats } from "../db.mjs";
import { errMsg, today } from "../util.mjs";
import { runDigestPipeline } from "./passes.mjs";

export function saveDigestMarkdown(day, prose) {
  const mdPath = join(PATHS.digestDir(), `${day}.md`);
  writeFileSync(mdPath, prose ?? "", "utf8");
  return mdPath;
}

export function needsDigestRefresh(db, day) {
  const stats = witnessStats(db, day);
  if (stats.frames === 0 && stats.utterances === 0 && stats.speech === 0) {
    return { run: false, reason: "no_witness_data" };
  }

  const latest = latestWitnessAt(db, day);
  if (!latest) {
    return { run: false, reason: "no_timestamps" };
  }

  const existing = getDigest(db, day);
  if (!existing?.prose) {
    return { run: true, reason: "first_digest", stats, latest };
  }

  if (latest > existing.updated_at) {
    return { run: true, reason: "new_witness_data", stats, latest };
  }

  return { run: false, reason: "up_to_date", stats, latest };
}

export async function runDigestForDay(db, day, { force = false } = {}) {
  const check = needsDigestRefresh(db, day);
  if (!force && !check.run) {
    return { skipped: true, day, reason: check.reason };
  }

  const result = await runDigestPipeline(db, day);
  const mdPath = saveDigestMarkdown(day, result.prose);
  console.log(`[digest] ${day} saved ${mdPath} (${check.reason ?? "forced"})`);
  return { skipped: false, day, prose: result.prose, path: mdPath };
}

export function startDigestLoop(intervalMs = CFG.digestIntervalMs) {
  let busy = false;
  let lastDay = today();

  console.log(`[digest] auto every ${intervalMs}ms — refreshes when new witness data arrives`);

  const tick = async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const db = openDb();
      const currentDay = today();

      if (currentDay !== lastDay) {
        const prior = lastDay;
        lastDay = currentDay;
        console.log(`[digest] day rollover — finalizing ${prior}`);
        await runDigestForDay(db, prior, { force: true });
      }

      await runDigestForDay(db, currentDay);
    } catch (err) {
      console.error(`[digest] ${errMsg(err)}`);
    } finally {
      busy = false;
    }
  };

  setInterval(tick, intervalMs);
  setTimeout(tick, 20000);
}
