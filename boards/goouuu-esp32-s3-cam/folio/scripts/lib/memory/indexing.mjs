import { CFG } from "../config/index.mjs";
import {
  deleteMemoryForDay,
  episodeSummariesForDay,
  getDigest,
  graphNodesForDay,
  insertMemoryChunk,
  openDb,
  upsertProfileFact,
} from "../db/index.mjs";
import { isoNow } from "../util/time.mjs";
import { embedText, serializeEmbedding } from "./embeddings.mjs";

const DIGEST_FACT_RULES = {
  pattern: { category: "pattern", confidence: 0.65, memoryKind: "pattern", weight: 0.85 },
  decision: { category: "decision", confidence: 0.75, memoryKind: "decision", weight: 1.25 },
  open_loop: { category: "open", confidence: 0.7, memoryKind: "open_loop", weight: 1.1 },
  claim: { category: "claim", confidence: 0.8, memoryKind: "claim", weight: 1.2 },
};

export function factLongEnough(text) {
  return String(text ?? "").trim().length >= CFG.memoryMinFactTextLength;
}

export function profileSlug(category, text) {
  return `${category}:${String(text).slice(0, 48).replace(/\s+/g, "_")}`;
}

function* iterDigestProfileFacts(passB, passC) {
  for (const p of passB?.patterns ?? []) {
    if (typeof p === "string" && factLongEnough(p)) {
      const rule = DIGEST_FACT_RULES.pattern;
      yield { ...rule, text: p };
    }
  }

  for (const d of passB?.decisions_real ?? []) {
    const text = d.text ?? d;
    if (typeof text === "string" && factLongEnough(text)) {
      const rule = DIGEST_FACT_RULES.decision;
      yield {
        ...rule,
        text,
        confidence: d.confidence ?? rule.confidence,
        evidence: d.evidence ?? [],
      };
    }
  }

  for (const loop of passB?.open_loops ?? []) {
    if (typeof loop === "string" && factLongEnough(loop)) {
      const rule = DIGEST_FACT_RULES.open_loop;
      yield { ...rule, text: loop };
    }
  }

  for (const c of passC?.approved_claims ?? []) {
    if (factLongEnough(c.text)) {
      const rule = DIGEST_FACT_RULES.claim;
      yield {
        ...rule,
        text: c.text,
        confidence: c.confidence ?? rule.confidence,
        evidence: c.evidence ?? [],
      };
    }
  }

  for (const cm of passC?.approved_cross_modal ?? []) {
    if (factLongEnough(cm.inference)) {
      const rule = DIGEST_FACT_RULES.claim;
      yield {
        ...rule,
        text: cm.inference,
        confidence: cm.confidence ?? rule.confidence,
        evidence: cm.evidence ?? [],
      };
    }
  }
}

export function syncDigestProfile(db, day, passB, passC) {
  for (const fact of iterDigestProfileFacts(passB, passC)) {
    upsertProfileFact(db, profileSlug(fact.category, fact.text), fact.text, day, fact.confidence);
  }
}

export function push(items, kind, day, text, evidence = [], weight = 1) {
  const t = String(text ?? "").trim();
  if (t.length < CFG.memoryMinFactTextLength) {
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
  syncDigestProfile(db, day, payload.passB, payload.passC);

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
    const { indexed } = await indexDayMemories(db, day, {
      episodes: episodeSummariesForDay(db, day),
      passB: JSON.parse(digest.pass_b_json || "{}"),
      passC: JSON.parse(digest.pass_c_json || "{}"),
      prose: digest.prose,
      graphNodes: graphNodesForDay(db, day),
    });
    total += indexed;
  }

  return { days: days.length, chunks: total };
}
