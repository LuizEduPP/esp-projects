
export function insertEvent(db, row) {
  db.prepare(
    `INSERT INTO events (device_id, at, kind, payload_json)
     VALUES (@device_id, @at, @kind, @payload_json)`,
  ).run(row);
}

export function eventsForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(`SELECT * FROM events WHERE at >= ? AND at < ? ORDER BY at`)
    .all(start, end);
}

export function pendingCounts(db) {
  const audio = db
    .prepare("SELECT COUNT(*) AS n FROM audio_chunks WHERE processed = 0")
    .get().n;
  const frames = db.prepare("SELECT COUNT(*) AS n FROM frames WHERE processed = 0").get().n;
  return { audio, frames };
}
