import { CFG } from "../config/index.mjs";
import { dayOffset } from "../util/time.mjs";
import { memoryChunksInRange, openDb } from "../db/index.mjs";
import { embedText, scorePair } from "./embeddings.mjs";

export async function retrieveMemories(db, query, { day, limit = CFG.memoryRetrieveLimit } = {}) {
  if (!query?.trim()) {
    return [];
  }

  const beforeDay = day ?? new Date().toISOString().slice(0, 10);
  const minDay = dayOffset(beforeDay, -CFG.memoryLookbackDays);
  const candidates = memoryChunksInRange(db, minDay, beforeDay);

  const queryEmbed = await embedText(query);
  const scored = candidates
    .map((c) => ({
      ...c,
      score: scorePair(queryEmbed, c.embedding_json),
    }))
    .filter((c) => c.score >= CFG.memoryMinScore)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit);

  return scored;
}

export async function retrieveContextForDay(db, day, { query = "", limit = CFG.memoryRetrieveLimit } = {}) {
  const q =
    query ||
    String(CFG.memoryContextQueryTemplate ?? "").replaceAll("{day}", day).trim();
  return retrieveMemories(db, q, { day, limit });
}
