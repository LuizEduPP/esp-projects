import { dayBounds } from "../util/time.mjs";

export function insertFrame(db, row) {
  const info = db
    .prepare(
      `INSERT INTO frames (device_id, captured_at, path, reason, processed)
       VALUES (@device_id, @captured_at, @path, @reason, 0)`,
    )
    .run(row);
  return Number(info.lastInsertRowid);
}

export function pendingFrames(db, limit = 10) {
  return db
    .prepare(`SELECT * FROM frames WHERE processed = 0 ORDER BY captured_at ASC LIMIT ?`)
    .all(limit);
}

export function markFrameProcessed(db, id, caption, sceneJson) {
  db.prepare(
    `UPDATE frames SET processed = 1, caption = @caption, scene_json = @scene_json WHERE id = @id`,
  ).run({ id, caption, scene_json: sceneJson });
}

export function getFrame(db, id) {
  return db.prepare("SELECT * FROM frames WHERE id = ?").get(id);
}

export function lastProcessedFrame(db) {
  return db
    .prepare(
      `SELECT id, caption, scene_json, captured_at, path, reason FROM frames
       WHERE processed = 1
       ORDER BY captured_at DESC LIMIT 1`,
    )
    .get();
}

export function framesForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT * FROM frames WHERE captured_at >= ? AND captured_at < ? ORDER BY captured_at`,
    )
    .all(start, end);
}
