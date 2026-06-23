import { CFG } from "../config/index.mjs";
import { isoNow } from "../util/time.mjs";

export function ensureDevice(db, deviceId, label = null) {
  const row = db.prepare("SELECT id FROM devices WHERE id = ?").get(deviceId);
  if (row) {
    return;
  }
  db.prepare("INSERT INTO devices (id, label, created_at) VALUES (?, ?, ?)").run(
    deviceId,
    label ?? deviceId,
    isoNow(),
  );
}

export function touchDevice(db, deviceId, { configVersion = null } = {}) {
  ensureDevice(db, deviceId);
  const now = isoNow();
  if (configVersion) {
    db.prepare(
      `UPDATE devices SET last_seen_at = ?, node_config_applied = ? WHERE id = ?`,
    ).run(now, configVersion, deviceId);
    return;
  }
  db.prepare(`UPDATE devices SET last_seen_at = ? WHERE id = ?`).run(now, deviceId);
}

export function listDevices(db) {
  return db
    .prepare(
      `SELECT id, label, last_seen_at, node_config_version, node_config_applied, created_at
       FROM devices ORDER BY last_seen_at DESC`,
    )
    .all();
}

export function upsertSpeaker(db, speakerId, displayName, profileJson = null) {
  db.prepare(
    `INSERT OR REPLACE INTO speakers (id, display_name, profile_json, embedding_path, created_at)
     VALUES (?, ?, ?, ?, ?)`,
  ).run(
    speakerId,
    displayName,
    JSON.stringify(profileJson ?? { locale: CFG.defaultLocale }),
    null,
    isoNow(),
  );
}
