import { getDigest, graphNodesForDay, openDb, episodeSummariesForDay } from "../db.mjs";
import { indexDayMemories } from "./index.mjs";

export async function reindexMemoriesFromDigests(db = openDb()) {
  const days = db
    .prepare("SELECT day FROM digests WHERE prose IS NOT NULL ORDER BY day")
    .all()
    .map((r) => r.day);

  let total = 0;
  for (const day of days) {
    const digest = getDigest(db, day);
    if (!digest?.prose) {
      continue;
    }
    const passB = JSON.parse(digest.pass_b_json || "{}");
    const passC = JSON.parse(digest.pass_c_json || "{}");
    const episodes = episodeSummariesForDay(db, day);
    const { indexed } = await indexDayMemories(db, day, {
      episodes,
      passB,
      passC,
      prose: digest.prose,
      graphNodes: graphNodesForDay(db, day),
    });
    total += indexed;
  }

  return { days: days.length, chunks: total };
}
