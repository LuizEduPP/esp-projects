import { CFG } from "./config.mjs";
import { LEXICAL_MIN_TOKEN_LENGTH, lexicalStopWordSet } from "./locale.mjs";
import {
  deleteMemoryForDay,
  episodeSummariesForDay,
  getDigest,
  graphNodesBeforeDay,
  graphNodesForDay,
  insertMemoryChunk,
  memoryChunksInRange,
  openDb,
  profileFacts,
  upsertProfileFact,
} from "./db.mjs";
import { dayOffset, isoNow } from "./util.mjs";


function embeddingsUrl() {
  if (CFG.memoryEmbeddingsUrl) {
    return CFG.memoryEmbeddingsUrl;
  }
  return CFG.lmUrl.replace(/\/chat\/completions\/?$/, "/embeddings");
}

export function tokenize(text) {
  const stop = lexicalStopWordSet();
  return String(text ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^a-z0-9à-ú]+/gi, " ")
    .split(/\s+/)
    .filter((t) => t.length >= LEXICAL_MIN_TOKEN_LENGTH && !stop.has(t));
}

export function termVector(text) {
  const vec = new Map();
  for (const tok of tokenize(text)) {
    vec.set(tok, (vec.get(tok) ?? 0) + 1);
  }
  return vec;
}

export function cosineSimilarity(a, b) {
  if (!a.size || !b.size) {
    return 0;
  }
  let dot = 0;
  let normA = 0;
  let normB = 0;
  for (const v of a.values()) {
    normA += v * v;
  }
  for (const v of b.values()) {
    normB += v * v;
  }
  const smaller = a.size < b.size ? a : b;
  const larger = a.size < b.size ? b : a;
  for (const [k, v] of smaller) {
    const w = larger.get(k);
    if (w) {
      dot += v * w;
    }
  }
  if (!normA || !normB) {
    return 0;
  }
  return dot / (Math.sqrt(normA) * Math.sqrt(normB));
}

export function cosineDense(a, b) {
  if (!a?.length || !b?.length || a.length !== b.length) {
    return 0;
  }
  let dot = 0;
  let normA = 0;
  let normB = 0;
  for (let i = 0; i < a.length; i++) {
    dot += a[i] * b[i];
    normA += a[i] * a[i];
    normB += b[i] * b[i];
  }
  if (!normA || !normB) {
    return 0;
  }
  return dot / (Math.sqrt(normA) * Math.sqrt(normB));
}

export function vectorFromJson(raw) {
  if (!raw) {
    return null;
  }
  try {
    const parsed = JSON.parse(raw);
    if (Array.isArray(parsed) && typeof parsed[0] === "number") {
      return parsed;
    }
    if (Array.isArray(parsed) && Array.isArray(parsed[0])) {
      return new Map(parsed);
    }
    return null;
  } catch {
    return null;
  }
}

export async function embedText(text) {
  if (!CFG.memoryUseEmbeddings) {
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }

  try {
    const res = await fetch(embeddingsUrl(), {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        model: CFG.memoryEmbeddingModel || CFG.modelFast,
        input: text.slice(0, 2000),
      }),
      signal: AbortSignal.timeout(60_000),
    });
    if (!res.ok) {
      throw new Error(`embeddings ${res.status}`);
    }
    const json = await res.json();
    const vec = json?.data?.[0]?.embedding;
    if (!Array.isArray(vec)) {
      throw new Error("no embedding vector");
    }
    return { kind: "float", vector: vec };
  } catch (err) {
    if (!CFG.memoryFallbackLexical) {
      throw new Error(`embeddings failed: ${err.message}`);
    }
    console.warn(`[memory] embeddings failed, fallback lexical: ${err.message}`);
    return { kind: "lexical", vector: [...termVector(text).entries()] };
  }
}

export function scorePair(queryEmbed, docEmbedJson, docText) {
  const stored = vectorFromJson(docEmbedJson);

  if (queryEmbed.kind === "float" && Array.isArray(stored)) {
    return cosineDense(queryEmbed.vector, stored);
  }

  const queryMap =
    queryEmbed.kind === "lexical" ? new Map(queryEmbed.vector) : termVector(docText);
  const docMap = stored instanceof Map ? stored : termVector(docText);
  return cosineSimilarity(queryMap, docMap);
}

export function serializeEmbedding(embed) {
  return JSON.stringify(embed.vector);
}

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
  return rows
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
}

export function retrieveGraphContext(db, day, queryText, limit = CFG.memoryGraphRetrieveLimit) {
  const tokens = new Set(tokenize(queryText));
  if (!tokens.size) {
    return [];
  }

  return graphNodesBeforeDay(db, day, CFG.memoryLookbackDays)
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
    .filter((n) => n.score >= CFG.memoryGraphMinScore)
    .sort((a, b) => b.score - a.score)
    .slice(0, limit)
    .map((n) => ({
      day: n.day,
      kind: n.kind,
      label: n.label,
      score: Number(n.score.toFixed(2)),
    }));
}

export async function buildRagContext(db, day, { episodes, passAJson }) {
  const query = buildMemoryQuery(day, episodes, passAJson);
  const [memories, graph, profile] = await Promise.all([
    retrieveMemories(db, query, { day }),
    Promise.resolve(retrieveGraphContext(db, day, query)),
    Promise.resolve(profileFacts(db).slice(0, CFG.memoryProfileLimit)),
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
