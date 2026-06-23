import { CFG } from "../config/index.mjs";
import { dayOffset, isoNow, priorDay } from "../util/time.mjs";
import { sqlNullable, sqlText } from "./sql.mjs";

export function getDigest(db, day) {
  return db.prepare("SELECT * FROM digests WHERE day = ?").get(day);
}

export function upsertDigest(db, row) {
  const existing = getDigest(db, row.day);
  if (existing) {
    db.prepare(
      `UPDATE digests SET
        pass_a_json = COALESCE(@pass_a_json, pass_a_json),
        pass_b_json = COALESCE(@pass_b_json, pass_b_json),
        pass_c_json = COALESCE(@pass_c_json, pass_c_json),
        prose = COALESCE(@prose, prose),
        evidence_json = COALESCE(@evidence_json, evidence_json),
        model_fast = COALESCE(@model_fast, model_fast),
        model_deep = COALESCE(@model_deep, model_deep),
        updated_at = @updated_at
       WHERE day = @day`,
    ).run({
      day: row.day,
      pass_a_json: row.pass_a_json ?? null,
      pass_b_json: row.pass_b_json ?? null,
      pass_c_json: row.pass_c_json ?? null,
      prose: row.prose ?? null,
      evidence_json: row.evidence_json ?? null,
      model_fast: row.model_fast ?? null,
      model_deep: row.model_deep ?? null,
      updated_at: row.updated_at,
    });
    return;
  }
  db.prepare(
    `INSERT INTO digests (
      day, pass_a_json, pass_b_json, pass_c_json, prose, evidence_json,
      model_fast, model_deep, created_at, updated_at
    ) VALUES (
      @day, @pass_a_json, @pass_b_json, @pass_c_json, @prose, @evidence_json,
      @model_fast, @model_deep, @created_at, @updated_at
    )`,
  ).run({
    day: row.day,
    pass_a_json: row.pass_a_json ?? null,
    pass_b_json: row.pass_b_json ?? null,
    pass_c_json: row.pass_c_json ?? null,
    prose: row.prose ?? null,
    evidence_json: row.evidence_json ?? null,
    model_fast: row.model_fast ?? null,
    model_deep: row.model_deep ?? null,
    created_at: row.created_at,
    updated_at: row.updated_at,
  });
}

/** Save partial pipeline output so UI can show draft while Pass D runs. */
export function patchDigest(db, day, patch) {
  const now = isoNow();
  const existing = getDigest(db, day);
  upsertDigest(db, {
    day,
    pass_a_json: patch.pass_a_json ?? existing?.pass_a_json ?? null,
    pass_b_json: patch.pass_b_json ?? existing?.pass_b_json ?? null,
    pass_c_json: patch.pass_c_json ?? existing?.pass_c_json ?? null,
    prose: patch.prose ?? existing?.prose ?? null,
    evidence_json: patch.evidence_json ?? existing?.evidence_json ?? null,
    model_fast: patch.model_fast ?? existing?.model_fast ?? null,
    model_deep: patch.model_deep ?? existing?.model_deep ?? null,
    created_at: existing?.created_at ?? now,
    updated_at: now,
  });
}

export function getDayRollup(db, day) {
  return db.prepare("SELECT * FROM day_rollups WHERE day = ?").get(day);
}

export function upsertDayRollup(db, day, compactJson) {
  const now = isoNow();
  const existing = getDayRollup(db, day);
  if (existing) {
    db.prepare(
      `UPDATE day_rollups SET compact_json = @compact_json, created_at = @created_at WHERE day = @day`,
    ).run({ day, compact_json: compactJson, created_at: now });
    return;
  }
  db.prepare(
    `INSERT INTO day_rollups (day, compact_json, created_at) VALUES (@day, @compact_json, @created_at)`,
  ).run({ day, compact_json: compactJson, created_at: now });
}

export function priorDayRollup(db, day) {
  const rollup = getDayRollup(db, priorDay(day));
  return rollup ? JSON.parse(rollup.compact_json) : null;
}

export function profileFacts(db) {
  return db.prepare("SELECT key, value, confidence FROM profile_facts ORDER BY key").all();
}

export function upsertProfileFact(db, key, value, sourceDay, confidence) {
  const now = isoNow();
  const existing = db.prepare("SELECT id FROM profile_facts WHERE key = ?").get(key);
  if (existing) {
    db.prepare(
      `UPDATE profile_facts SET value = ?, source_day = ?, confidence = ?, updated_at = ? WHERE key = ?`,
    ).run(value, sourceDay, confidence, now, key);
    return;
  }
  db.prepare(
    `INSERT INTO profile_facts (key, value, source_day, confidence, updated_at)
     VALUES (?, ?, ?, ?, ?)`,
  ).run(key, value, sourceDay, confidence, now);
}

export function deleteMemoryForDay(db, day) {
  db.prepare("DELETE FROM memory_chunks WHERE day = ?").run(day);
}

export function insertMemoryChunk(db, row) {
  db.prepare(
    `INSERT INTO memory_chunks (day, kind, text, evidence_json, embedding_json, weight, created_at)
     VALUES (@day, @kind, @text, @evidence_json, @embedding_json, @weight, @created_at)`,
  ).run(row);
}

export function memoryChunksInRange(db, minDay, beforeDay) {
  return db
    .prepare(
      `SELECT * FROM memory_chunks WHERE day >= ? AND day < ? ORDER BY day DESC`,
    )
    .all(minDay, beforeDay);
}

export function memoryChunkCount(db) {
  return db.prepare("SELECT COUNT(*) AS n FROM memory_chunks").get().n;
}

export function graphNodesForDay(db, day) {
  return db.prepare("SELECT * FROM graph_nodes WHERE day = ?").all(day);
}

export function graphNodesBeforeDay(db, beforeDay, lookbackDays = CFG.memoryLookbackDays) {
  const minDay = dayOffset(beforeDay, -lookbackDays);
  return db
    .prepare(`SELECT * FROM graph_nodes WHERE day >= ? AND day < ?`)
    .all(minDay, beforeDay);
}

export function insertGraphNode(db, node) {
  db.prepare(
    `INSERT OR REPLACE INTO graph_nodes (id, day, kind, label, payload_json)
     VALUES (@id, @day, @kind, @label, @payload_json)`,
  ).run({
    id: sqlText(node.id),
    day: sqlText(node.day),
    kind: sqlText(node.kind),
    label: sqlText(node.label, "node"),
    payload_json: sqlNullable(node.payload_json),
  });
}

export function insertGraphEdge(db, edge) {
  db.prepare(
    `INSERT INTO graph_edges (day, from_node, to_node, relation, evidence_json, confidence)
     VALUES (@day, @from_node, @to_node, @relation, @evidence_json, @confidence)`,
  ).run({
    day: sqlText(edge.day),
    from_node: sqlText(edge.from_node),
    to_node: sqlText(edge.to_node),
    relation: sqlText(edge.relation),
    evidence_json: sqlNullable(edge.evidence_json),
    confidence: sqlNullable(edge.confidence),
  });
}
