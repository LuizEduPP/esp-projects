import { CFG } from "../config.mjs";
import { deleteMemoryForDay, insertMemoryChunk } from "../db.mjs";
import { isoNow } from "../util.mjs";
import { iterDigestProfileFacts } from "./digest-facts.mjs";
import { embedText, serializeEmbedding } from "./embed.mjs";

function push(items, kind, day, text, evidence = [], weight = 1) {
  const t = String(text ?? "").trim();
  if (t.length < 8) {
    return;
  }
  items.push({ kind, day, text: t.slice(0, 1200), evidence_json: JSON.stringify(evidence), weight });
}

export function collectMemoryItems(day, { episodes, passB, passC, prose, graphNodes }) {
  const items = [];

  for (const ep of episodes) {
    const s = ep.summary ?? {};
    push(items, "episode", day, `${ep.label}: ${(s.themes ?? []).join(", ")}`, [`ep:${ep.id}`], 1.1);
    for (const theme of s.themes ?? []) {
      push(items, "theme", day, theme, [`ep:${ep.id}`], 0.9);
    }
    for (const d of s.decisions ?? []) {
      push(items, "decision", day, d.text, d.evidence ?? [`ep:${ep.id}`], 1.2);
    }
    for (const q of s.open_questions ?? []) {
      push(items, "open_loop", day, q, [`ep:${ep.id}`], 1);
    }
    for (const q of s.notable_quotes ?? []) {
      push(items, "quote", day, q.text, q.evidence ?? [], 1.15);
    }
  }

  if (passB?.narrative_arc) {
    push(items, "arc", day, passB.narrative_arc, [], 1.3);
  }
  for (const fact of iterDigestProfileFacts(passB, passC)) {
    push(items, fact.memoryKind, day, fact.text, fact.evidence ?? [], fact.weight);
  }

  if (prose) {
    push(items, "digest", day, prose.slice(0, 600), [], 1.4);
  }

  for (const node of graphNodes ?? []) {
    push(items, `graph_${node.kind}`, day, node.label, [], 0.8);
  }

  return items;
}

export async function indexDayMemories(db, day, payload) {
  if (!CFG.memoryEnabled) {
    return { indexed: 0 };
  }

  const items = collectMemoryItems(day, payload);
  deleteMemoryForDay(db, day);

  let indexed = 0;
  for (const item of items) {
    const embed = await embedText(item.text);
    insertMemoryChunk(db, {
      day: item.day,
      kind: item.kind,
      text: item.text,
      evidence_json: item.evidence_json,
      embedding_json: serializeEmbedding(embed),
      weight: item.weight,
      created_at: isoNow(),
    });
    indexed++;
  }

  console.log(`[memory] indexed ${indexed} chunks for ${day}`);
  return { indexed };
}
