import { isoNow } from "../util/time.mjs";
import { sqlNullable, sqlText } from "./sql.mjs";

export function listEntities(db) {
  return db
    .prepare("SELECT * FROM entities ORDER BY kind, display_name")
    .all();
}

export function getEntity(db, id) {
  return db.prepare("SELECT * FROM entities WHERE id = ?").get(id);
}

export function entityBySpeakerId(db, speakerId) {
  return db.prepare("SELECT * FROM entities WHERE speaker_id = ?").get(speakerId);
}

export function upsertEntity(db, entity) {
  const now = isoNow();
  const existing = getEntity(db, entity.id);
  if (existing) {
    db.prepare(
      `UPDATE entities SET
        kind = @kind,
        display_name = @display_name,
        speaker_id = @speaker_id,
        profile_json = @profile_json,
        patterns_json = COALESCE(@patterns_json, patterns_json),
        updated_at = @updated_at
       WHERE id = @id`,
    ).run({
      id: sqlText(entity.id),
      kind: sqlText(entity.kind),
      display_name: sqlText(entity.display_name),
      speaker_id: sqlNullable(entity.speaker_id),
      profile_json: sqlNullable(entity.profile_json),
      patterns_json: sqlNullable(entity.patterns_json),
      updated_at: now,
    });
    return;
  }
  db.prepare(
    `INSERT INTO entities (id, kind, display_name, speaker_id, profile_json, patterns_json, created_at, updated_at)
     VALUES (@id, @kind, @display_name, @speaker_id, @profile_json, @patterns_json, @created_at, @updated_at)`,
  ).run({
    id: sqlText(entity.id),
    kind: sqlText(entity.kind),
    display_name: sqlText(entity.display_name),
    speaker_id: sqlNullable(entity.speaker_id),
    profile_json: sqlNullable(entity.profile_json),
    patterns_json: sqlNullable(entity.patterns_json),
    created_at: now,
    updated_at: now,
  });
}

export function patchEntityPatterns(db, entityId, patterns) {
  db.prepare("UPDATE entities SET patterns_json = ?, updated_at = ? WHERE id = ?").run(
    JSON.stringify(patterns),
    isoNow(),
    entityId,
  );
}

export function entitiesActiveOnDay(db, day) {
  return listEntities(db).filter((e) => {
    try {
      const p = JSON.parse(e.patterns_json || "{}");
      return p.last_seen === day;
    } catch {
      return false;
    }
  });
}
