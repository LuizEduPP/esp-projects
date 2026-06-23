import { DatabaseSync } from "node:sqlite";
import { mkdirSync } from "node:fs";
import { CFG, PATHS } from "./config.mjs";
import { DB_SCHEMA, migrateDb } from "./db-schema.mjs";
import { dayBounds, dayOffset, isoNow, priorDay } from "./util.mjs";

let dbSingleton = null;

export function openDb() {
  if (dbSingleton) {
    return dbSingleton;
  }
  mkdirSync(CFG.dataDir, { recursive: true });
  mkdirSync(PATHS.speakerDir(), { recursive: true });
  mkdirSync(PATHS.digestDir(), { recursive: true });
  const db = new DatabaseSync(PATHS.db());
  db.exec("PRAGMA journal_mode = WAL;");
  db.exec("PRAGMA busy_timeout = 5000;");
  db.exec(DB_SCHEMA);
  migrateDb(db);
  dbSingleton = db;
  return db;
}

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

export function insertAudioChunk(db, row) {
  const info = db
    .prepare(
      `INSERT INTO audio_chunks (device_id, captured_at, seq, path, duration_ms, energy)
       VALUES (@device_id, @captured_at, @seq, @path, @duration_ms, @energy)`,
    )
    .run(row);
  return Number(info.lastInsertRowid);
}

export function insertFrame(db, row) {
  const info = db
    .prepare(
      `INSERT INTO frames (device_id, captured_at, path, reason, processed)
       VALUES (@device_id, @captured_at, @path, @reason, 0)`,
    )
    .run(row);
  return Number(info.lastInsertRowid);
}

export function insertEvent(db, row) {
  db.prepare(
    `INSERT INTO events (device_id, at, kind, payload_json)
     VALUES (@device_id, @at, @kind, @payload_json)`,
  ).run(row);
}

export function insertUtterance(db, row) {
  const info = db
    .prepare(
      `INSERT INTO utterances (chunk_id, speaker_id, started_at, ended_at, text, confidence)
       VALUES (@chunk_id, @speaker_id, @started_at, @ended_at, @text, @confidence)`,
    )
    .run(row);
  return Number(info.lastInsertRowid);
}

export function pendingAudioChunks(db, limit = 20) {
  return db
    .prepare(
      `SELECT * FROM audio_chunks WHERE processed = 0
       ORDER BY CASE WHEN energy >= ? THEN 0 ELSE 1 END, captured_at ASC
       LIMIT ?`,
    )
    .all(CFG.speechEnergyThreshold, limit);
}

export function pendingFrames(db, limit = 10) {
  return db
    .prepare(`SELECT * FROM frames WHERE processed = 0 ORDER BY captured_at ASC LIMIT ?`)
    .all(limit);
}

export function pendingCounts(db) {
  const audio = db
    .prepare("SELECT COUNT(*) AS n FROM audio_chunks WHERE processed = 0")
    .get().n;
  const frames = db.prepare("SELECT COUNT(*) AS n FROM frames WHERE processed = 0").get().n;
  return { audio, frames };
}

export function markAudioProcessed(db, id) {
  db.prepare("UPDATE audio_chunks SET processed = 1 WHERE id = ?").run(id);
}

export function deleteAudioChunk(db, id) {
  db.prepare("DELETE FROM audio_chunks WHERE id = ?").run(id);
}

export function markFrameProcessed(db, id, caption, sceneJson) {
  db.prepare(
    `UPDATE frames SET processed = 1, caption = @caption, scene_json = @scene_json WHERE id = @id`,
  ).run({ id, caption, scene_json: sceneJson });
}

export function getAudioChunk(db, id) {
  return db.prepare("SELECT * FROM audio_chunks WHERE id = ?").get(id);
}

export function getFrame(db, id) {
  return db.prepare("SELECT * FROM frames WHERE id = ?").get(id);
}

export function audioChunksForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT c.*, u.text AS utterance_text
       FROM audio_chunks c
       LEFT JOIN utterances u ON u.chunk_id = c.id
       WHERE c.captured_at >= ? AND c.captured_at < ?
       ORDER BY c.captured_at`,
    )
    .all(start, end);
}

export function utterancesForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT * FROM utterances WHERE started_at >= ? AND started_at < ? ORDER BY started_at`,
    )
    .all(start, end);
}

export function framesForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT * FROM frames WHERE captured_at >= ? AND captured_at < ? ORDER BY captured_at`,
    )
    .all(start, end);
}

export function eventsForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(`SELECT * FROM events WHERE at >= ? AND at < ? ORDER BY at`)
    .all(start, end);
}

export function episodesForDay(db, day) {
  return db
    .prepare(`SELECT * FROM episodes WHERE day = ? ORDER BY started_at`)
    .all(day);
}

export function getDigest(db, day) {
  return db.prepare("SELECT * FROM digests WHERE day = ?").get(day);
}

export function upsertDigest(db, row) {
  const existing = getDigest(db, row.day);
  if (existing) {
    db.prepare(
      `UPDATE digests SET
        pass_a_json = @pass_a_json,
        pass_b_json = @pass_b_json,
        pass_c_json = @pass_c_json,
        prose = @prose,
        evidence_json = @evidence_json,
        model_fast = @model_fast,
        model_deep = @model_deep,
        updated_at = @updated_at
       WHERE day = @day`,
    ).run(row);
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
  ).run(row);
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

export function profileFacts(db) {
  return db.prepare("SELECT key, value, confidence FROM profile_facts ORDER BY key").all();
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

export function witnessStats(db, day) {
  const { start, end } = dayBounds(day);
  const speech = db
    .prepare(
      `SELECT COUNT(*) AS n FROM audio_chunks
       WHERE captured_at >= ? AND captured_at < ? AND energy >= ?`,
    )
    .get(start, end, CFG.speechEnergyThreshold).n;
  const frames = db
    .prepare(`SELECT COUNT(*) AS n FROM frames WHERE captured_at >= ? AND captured_at < ?`)
    .get(start, end).n;
  const utterances = db
    .prepare(`SELECT COUNT(*) AS n FROM utterances WHERE started_at >= ? AND started_at < ?`)
    .get(start, end).n;
  return { speech, frames, utterances };
}

export function latestWitnessAt(db, day) {
  const { start, end } = dayBounds(day);
  const row = db
    .prepare(
      `SELECT MAX(t) AS m FROM (
         SELECT MAX(captured_at) AS t FROM audio_chunks WHERE captured_at >= ? AND captured_at < ?
         UNION ALL
         SELECT MAX(captured_at) FROM frames WHERE captured_at >= ? AND captured_at < ?
         UNION ALL
         SELECT MAX(started_at) FROM utterances WHERE started_at >= ? AND started_at < ?
       )`,
    )
    .get(start, end, start, end, start, end);
  return row?.m ?? null;
}

export function alignedMomentsForDay(db, day) {
  const utterances = utterancesForDay(db, day).map((u) => ({
    id: `utt:${u.id}`,
    at: u.started_at,
    text: u.text,
  }));
  const frames = framesForDay(db, day).map((f) => ({
    id: `frm:${f.id}`,
    at: f.captured_at,
    visual: f.caption || f.scene_json,
  }));
  return [...utterances, ...frames].sort((a, b) => a.at.localeCompare(b.at));
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

export function priorDayRollup(db, day) {
  const rollup = getDayRollup(db, priorDay(day));
  return rollup ? JSON.parse(rollup.compact_json) : null;
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

/** Past days only — excludes `beforeDay` (today's digest indexes after retrieval). */
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

export function clearEpisodesForDay(db, day) {
  const eps = episodesForDay(db, day);
  for (const ep of eps) {
    db.prepare("DELETE FROM episode_utterances WHERE episode_id = ?").run(ep.id);
    db.prepare("DELETE FROM episode_frames WHERE episode_id = ?").run(ep.id);
  }
  db.prepare("DELETE FROM episodes WHERE day = ?").run(day);
  db.prepare("DELETE FROM graph_nodes WHERE day = ?").run(day);
  db.prepare("DELETE FROM graph_edges WHERE day = ?").run(day);
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

export function insertGraphNode(db, node) {
  db.prepare(
    `INSERT OR REPLACE INTO graph_nodes (id, day, kind, label, payload_json)
     VALUES (@id, @day, @kind, @label, @payload_json)`,
  ).run(node);
}

export function insertGraphEdge(db, edge) {
  db.prepare(
    `INSERT INTO graph_edges (day, from_node, to_node, relation, evidence_json, confidence)
     VALUES (@day, @from_node, @to_node, @relation, @evidence_json, @confidence)`,
  ).run(edge);
}

export function timelineForDay(db, day) {
  const audio = audioChunksForDay(db, day)
    .filter((c) => !c.processed || c.utterance_text)
    .map((c) => ({
    type: "audio",
    at: c.captured_at,
    id: `aud:${c.id}`,
    chunk_id: c.id,
    energy: c.energy,
    speech: (c.energy ?? 0) >= CFG.speechEnergyThreshold,
    text: c.utterance_text ?? null,
    processed: !!c.processed,
  }));
  const frames = framesForDay(db, day).map((f) => ({
    type: "frame",
    at: f.captured_at,
    id: `frm:${f.id}`,
    frame_id: f.id,
    caption: f.caption,
    reason: f.reason,
    processed: !!f.processed,
  }));
  return [...audio, ...frames].sort((a, b) => a.at.localeCompare(b.at));
}
