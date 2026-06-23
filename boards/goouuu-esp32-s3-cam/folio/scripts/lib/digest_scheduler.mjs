import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "./config.mjs";
import { getDigest, openDb } from "./db.mjs";
import { runDigestPipeline } from "./passes.mjs";
import { errMsg } from "./util.mjs";

export function saveDigestMarkdown(day, prose) {
  const mdPath = join(PATHS.digestDir(), `${day}.md`);
  writeFileSync(mdPath, prose ?? "", "utf8");
  return mdPath;
}

export function witnessStats(db, day) {
  const start = `${day}T00:00:00.000Z`;
  const end = `${day}T23:59:59.999Z`;
  const speech = db
    .prepare(
      `SELECT COUNT(*) AS n FROM audio_chunks
       WHERE captured_at >= ? AND captured_at < ? AND energy >= 0.008`,
    )
    .get(start, end).n;
  const frames = db
    .prepare(`SELECT COUNT(*) AS n FROM frames WHERE captured_at >= ? AND captured_at < ?`)
    .get(start, end).n;
  const utterances = db
    .prepare(`SELECT COUNT(*) AS n FROM utterances WHERE started_at >= ? AND started_at < ?`)
    .get(start, end).n;
  return { speech, frames, utterances };
}

export function latestWitnessAt(db, day) {
  const start = `${day}T00:00:00.000Z`;
  const end = `${day}T23:59:59.999Z`;
  const row = db
    .prepare(
      `SELECT MAX(t) AS m FROM (
         SELECT MAX(captured_at) AS t FROM audio_chunks WHERE captured_at >= ? AND captured_at < ?
         UNION ALL
         SELECT MAX(captured_at) FROM frames WHERE captured_at >= ? AND captured_at < ?
         UNION ALL
         SELECT MAX(started_at) FROM utterances WHERE started_at >= ? AND started_at < ?
       )`,
    )
    .get(start, end, start, end, start, end);
  return row?.m ?? null;
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
  let lastDay = new Date().toISOString().slice(0, 10);

  console.log(`[digest] auto every ${intervalMs}ms — refreshes when new witness data arrives`);

  const tick = async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const db = openDb();
      const today = new Date().toISOString().slice(0, 10);

      if (today !== lastDay) {
        const prior = lastDay;
        lastDay = today;
        console.log(`[digest] day rollover — finalizing ${prior}`);
        await runDigestForDay(db, prior, { force: true });
      }

      await runDigestForDay(db, today);
    } catch (err) {
      console.error(`[digest] ${errMsg(err)}`);
    } finally {
      busy = false;
    }
  };

  setInterval(tick, intervalMs);
  setTimeout(tick, 20000);
}
