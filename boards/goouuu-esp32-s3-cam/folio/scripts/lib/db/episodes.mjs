export function episodesForDay(db, day) {
  return db
    .prepare(`SELECT * FROM episodes WHERE day = ? ORDER BY started_at`)
    .all(day);
}

export function clearEpisodesForDay(db, day) {
  const eps = episodesForDay(db, day);
  for (const ep of eps) {
    db.prepare("DELETE FROM episode_utterances WHERE episode_id = ?").run(ep.id);
    db.prepare("DELETE FROM episode_frames WHERE episode_id = ?").run(ep.id);
  }
  db.prepare("DELETE FROM episodes WHERE day = ?").run(day);
  db.prepare("DELETE FROM graph_edges WHERE day = ?").run(day);
  db.prepare("DELETE FROM graph_nodes WHERE day = ?").run(day);
}

export function insertEpisode(db, ep) {
  db.prepare(
    `INSERT INTO episodes (id, day, started_at, ended_at, label, summary_json, created_at)
     VALUES (@id, @day, @started_at, @ended_at, @label, @summary_json, @created_at)`,
  ).run(ep);
}

export function linkEpisodeUtterance(db, episodeId, utteranceId) {
  db.prepare(
    `INSERT OR IGNORE INTO episode_utterances (episode_id, utterance_id) VALUES (?, ?)`,
  ).run(episodeId, utteranceId);
}

export function linkEpisodeFrame(db, episodeId, frameId) {
  db.prepare(
    `INSERT OR IGNORE INTO episode_frames (episode_id, frame_id) VALUES (?, ?)`,
  ).run(episodeId, frameId);
}

export function episodeSummariesForDay(db, day) {
  return episodesForDay(db, day).map((ep) => ({
    id: ep.id,
    label: ep.label,
    started_at: ep.started_at,
    ended_at: ep.ended_at,
    summary: JSON.parse(ep.summary_json || "{}"),
  }));
}
