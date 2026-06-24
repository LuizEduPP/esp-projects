import { CFG } from "../config.mjs";
import { isoNow } from "../util.mjs";
import { getSpeaker, upsertSpeaker } from "./speakers.mjs";
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

export function speakerEntityId(speakerId) {
  return speakerId ? `speaker:${speakerId}` : null;
}

/** Ensure speakers row exists before entities.speaker_id FK insert. */
function ensureSpeakerForEntity(db, speakerId, displayName) {
  if (!speakerId || getSpeaker(db, speakerId)) {
    return;
  }
  upsertSpeaker(db, speakerId, displayName || speakerId);
}

export function ensureSpeakerEntity(db, speakerId, displayName) {
  if (!speakerId) {
    return null;
  }
  const entityId = speakerEntityId(speakerId);
  ensureSpeakerForEntity(db, speakerId, displayName);
  upsertEntity(db, {
    id: entityId,
    kind: "person",
    display_name: displayName || getSpeaker(db, speakerId)?.display_name || speakerId,
    speaker_id: speakerId,
    profile_json: JSON.stringify({ speaker_id: speakerId }),
    patterns_json: null,
  });
  return entityId;
}

export function upsertEntity(db, entity) {
  const now = isoNow();
  const speakerId = sqlNullable(entity.speaker_id);
  if (speakerId) {
    ensureSpeakerForEntity(db, speakerId, entity.display_name);
  }
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
