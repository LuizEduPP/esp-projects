import { isoNow } from "../util/time.mjs";
import { sqlNullable, sqlText } from "./sql.mjs";

export function getDayInsights(db, day) {
  return db.prepare("SELECT * FROM day_insights WHERE day = ?").get(day);
}

export function upsertDayInsights(db, row) {
  const existing = getDayInsights(db, row.day);
  if (existing) {
    db.prepare(
      `UPDATE day_insights SET
        stats_json = @stats_json,
        insights_json = @insights_json,
        entities_json = @entities_json,
        model = @model,
        updated_at = @updated_at
       WHERE day = @day`,
    ).run({
      day: row.day,
      stats_json: row.stats_json,
      insights_json: row.insights_json,
      entities_json: row.entities_json ?? null,
      model: row.model ?? null,
      updated_at: row.updated_at,
    });
    return;
  }
  db.prepare(
    `INSERT INTO day_insights (day, stats_json, insights_json, entities_json, model, created_at, updated_at)
     VALUES (@day, @stats_json, @insights_json, @entities_json, @model, @created_at, @updated_at)`,
  ).run({
    day: row.day,
    stats_json: row.stats_json,
    insights_json: row.insights_json,
    entities_json: row.entities_json ?? null,
    model: row.model ?? null,
    created_at: row.created_at,
    updated_at: row.updated_at,
  });
}

export function dayInsightDays(db) {
  return db.prepare("SELECT day FROM day_insights ORDER BY day DESC").all().map((r) => r.day);
}

export function latestInsightUpdate(db, day) {
  return getDayInsights(db, day)?.updated_at ?? null;
}

export function witnessIndexedAt(db, day) {
  const row = db
    .prepare("SELECT MAX(created_at) AS m FROM memory_chunks WHERE day = ?")
    .get(day);
  return row?.m ?? null;
}
