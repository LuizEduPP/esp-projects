import { CFG } from "../config/index.mjs";
import { isoNow } from "../util/time.mjs";

export function listSpeakers(db) {
  return db.prepare("SELECT * FROM speakers ORDER BY display_name").all();
}

export function getSpeaker(db, id) {
  return db.prepare("SELECT * FROM speakers WHERE id = ?").get(id);
}

export function upsertSpeaker(db, speakerId, displayName, profile = null) {
  const existing = getSpeaker(db, speakerId);
  const profileJson = JSON.stringify(profile ?? { locale: CFG.defaultLocale });
  if (existing) {
    db.prepare(
      "UPDATE speakers SET display_name = ?, profile_json = ? WHERE id = ?",
    ).run(displayName, profileJson, speakerId);
    return;
  }
  db.prepare(
    `INSERT INTO speakers (id, display_name, profile_json, embedding_path, created_at)
     VALUES (?, ?, ?, ?, ?)`,
  ).run(speakerId, displayName, profileJson, null, isoNow());
}

export function updateSpeakerProfile(db, speakerId, profile) {
  db.prepare("UPDATE speakers SET profile_json = ? WHERE id = ?").run(
    JSON.stringify(profile),
    speakerId,
  );
}

export function speakersWithFingerprint(db) {
  return listSpeakers(db).filter((s) => {
    try {
      const p = JSON.parse(s.profile_json || "{}");
      return Array.isArray(p.fingerprint) && p.fingerprint.length > 0;
    } catch {
      return false;
    }
  });
}
