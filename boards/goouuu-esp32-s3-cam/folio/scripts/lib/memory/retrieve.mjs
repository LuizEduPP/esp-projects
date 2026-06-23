import { CFG } from "../config.mjs";
import { graphNodesBeforeDay, memoryChunksInRange, profileFacts } from "../db.mjs";
import { dayOffset } from "../util.mjs";
import { tokenize } from "./lexical.mjs";
import { embedText, scorePair } from "./embed.mjs";

export function buildMemoryQuery(day, episodes, passAJson = null) {
  const parts = [day];
  for (const ep of episodes) {
    parts.push(ep.label);
    const s = ep.summary ?? {};
    parts.push(...(s.themes ?? []));
    parts.push(...(s.decisions ?? []).map((d) => d.text));
    parts.push(...(s.open_questions ?? []));
  }
  if (passAJson?.events_notable) {
    parts.push(...passAJson.events_notable);
  }
  return parts.filter(Boolean).join(" · ");
}

export async function retrieveMemories(db, queryText, { day, limit = CFG.memoryRetrieveLimit } = {}) {
  if (!CFG.memoryEnabled || !queryText?.trim()) {
    return [];
  }

  const minDay = dayOffset(day, -CFG.memoryLookbackDays);
  const rows = memoryChunksInRange(db, minDay, day);
  if (!rows.length) {
    return [];
  }

  const queryEmbed = await embedText(queryText);
  const scored = rows
    .map((row) => ({
      id: row.id,
      day: row.day,
      kind: row.kind,
      text: row.text,
      evidence: JSON.parse(row.evidence_json || "[]"),
      score: scorePair(queryEmbed, row.embedding_json, row.text) * (row.weight ?? 1),
    }))
    .filter((r) => r.score >= CFG.memoryMinScore)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit);

  return scored;
}

export function retrieveGraphContext(db, day, queryText, limit = 8) {
  const tokens = new Set(tokenize(queryText));
  if (!tokens.size) {
    return [];
  }

  const nodes = graphNodesBeforeDay(db, day, CFG.memoryLookbackDays);
  const scored = nodes
    .map((n) => {
      const labelTokens = tokenize(n.label);
      let overlap = 0;
      for (const t of labelTokens) {
        if (tokens.has(t)) {
          overlap++;
        }
      }
      return { ...n, score: overlap / Math.max(labelTokens.length, 1) };
    })
    .filter((n) => n.score > 0.15)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit)
    .map((n) => ({
      day: n.day,
      kind: n.kind,
      label: n.label,
      score: Number(n.score.toFixed(2)),
    }));

  return scored;
}

export async function buildRagContext(db, day, { episodes, passAJson }) {
  const query = buildMemoryQuery(day, episodes, passAJson);
  const [memories, graph, profile] = await Promise.all([
    retrieveMemories(db, query, { day }),
    Promise.resolve(retrieveGraphContext(db, day, query)),
    Promise.resolve(profileFacts(db).slice(0, 32)),
  ]);

  if (memories.length) {
    console.log(
      `[memory] RAG ${memories.length} hits for "${query.slice(0, 60)}…" ` +
        `(top=${memories[0].day}/${memories[0].kind} score=${memories[0].score.toFixed(2)})`,
    );
  }

  return {
    query,
    memories: memories.map((m) => ({
      day: m.day,
      kind: m.kind,
      text: m.text,
      evidence: m.evidence,
      score: Number(m.score.toFixed(3)),
    })),
    graph,
    profile,
  };
}
