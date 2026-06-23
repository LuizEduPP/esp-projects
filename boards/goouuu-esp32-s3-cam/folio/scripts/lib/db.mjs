import { DatabaseSync } from "node:sqlite";
import { existsSync, mkdirSync, unlinkSync } from "node:fs";
import { CFG, PATHS } from "./config.mjs";
import { dayBounds, dayOffset, isoNow, retentionCutoffIso } from "./util.mjs";


// --- schema.mjs ---
/** SQLite schema — archive + entities + daily insights + RAG memory. */
export const DB_SCHEMA = `
CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  label TEXT,
  created_at TEXT NOT NULL,
  last_seen_at TEXT,
  node_config_version TEXT,
  node_config_applied TEXT
);

CREATE TABLE IF NOT EXISTS speakers (
  id TEXT PRIMARY KEY,
  display_name TEXT NOT NULL,
  profile_json TEXT,
  embedding_path TEXT,
  created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS entities (
  id TEXT PRIMARY KEY,
  kind TEXT NOT NULL,
  display_name TEXT NOT NULL,
  speaker_id TEXT,
  profile_json TEXT,
  patterns_json TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL,
  FOREIGN KEY (speaker_id) REFERENCES speakers(id)
);
CREATE INDEX IF NOT EXISTS idx_entities_kind ON entities(kind);

CREATE TABLE IF NOT EXISTS audio_chunks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT NOT NULL,
  captured_at TEXT NOT NULL,
  seq INTEGER NOT NULL,
  path TEXT NOT NULL,
  duration_ms INTEGER NOT NULL DEFAULT 1000,
  energy REAL,
  device_ms INTEGER,
  stt_attempts INTEGER NOT NULL DEFAULT 0,
  processed INTEGER NOT NULL DEFAULT 0,
  sound_kind TEXT,
  sound_label TEXT,
  speaker_id TEXT,
  speaker_confidence REAL,
  FOREIGN KEY (device_id) REFERENCES devices(id),
  FOREIGN KEY (speaker_id) REFERENCES speakers(id)
);
CREATE INDEX IF NOT EXISTS idx_audio_chunks_day ON audio_chunks(captured_at);
CREATE INDEX IF NOT EXISTS idx_audio_chunks_processed ON audio_chunks(processed);

CREATE TABLE IF NOT EXISTS utterances (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  chunk_id INTEGER,
  speaker_id TEXT,
  started_at TEXT NOT NULL,
  ended_at TEXT NOT NULL,
  text TEXT NOT NULL,
  confidence REAL,
  FOREIGN KEY (chunk_id) REFERENCES audio_chunks(id),
  FOREIGN KEY (speaker_id) REFERENCES speakers(id)
);
CREATE INDEX IF NOT EXISTS idx_utterances_started ON utterances(started_at);

CREATE TABLE IF NOT EXISTS frames (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT NOT NULL,
  captured_at TEXT NOT NULL,
  path TEXT NOT NULL,
  reason TEXT,
  caption TEXT,
  scene_json TEXT,
  processed INTEGER NOT NULL DEFAULT 0
);
CREATE INDEX IF NOT EXISTS idx_frames_captured ON frames(captured_at);

CREATE TABLE IF NOT EXISTS events (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT NOT NULL,
  at TEXT NOT NULL,
  kind TEXT NOT NULL,
  payload_json TEXT
);
CREATE INDEX IF NOT EXISTS idx_events_at ON events(at);

CREATE TABLE IF NOT EXISTS day_insights (
  day TEXT PRIMARY KEY,
  stats_json TEXT NOT NULL,
  insights_json TEXT NOT NULL,
  entities_json TEXT,
  model TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS memory_chunks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  day TEXT NOT NULL,
  kind TEXT NOT NULL,
  text TEXT NOT NULL,
  evidence_json TEXT,
  embedding_json TEXT,
  entity_id TEXT,
  weight REAL NOT NULL DEFAULT 1.0,
  created_at TEXT NOT NULL,
  FOREIGN KEY (entity_id) REFERENCES entities(id)
);
CREATE INDEX IF NOT EXISTS idx_memory_chunks_day ON memory_chunks(day);
CREATE INDEX IF NOT EXISTS idx_memory_chunks_kind ON memory_chunks(kind);
`;

/** Legacy tables removed in migrateSchemaV2. */
export const LEGACY_TABLES = [
  "episode_frames",
  "episode_utterances",
  "episodes",
  "graph_edges",
  "graph_nodes",
  "digests",
  "day_rollups",
  "profile_facts",
];

// --- sql.mjs ---
/** SQLite rejects `undefined` bindings — normalize to null or string. */
export function sqlText(value, fallback = "") {
  if (value === undefined || value === null) {
    return fallback;
  }
  return String(value);
}

export function sqlNullable(value) {
  return value === undefined ? null : value;
}

// --- connection.mjs ---
function tableExists(db, name) {
  return Boolean(
    db.prepare("SELECT 1 FROM sqlite_master WHERE type='table' AND name=?").get(name),
  );
}

function migrateAudioChunkCols(db) {
  const cols = new Set(db.prepare("PRAGMA table_info(audio_chunks)").all().map((c) => c.name));
  const adds = [
    ["device_ms", "INTEGER"],
    ["stt_attempts", "INTEGER NOT NULL DEFAULT 0"],
    ["sound_kind", "TEXT"],
    ["sound_label", "TEXT"],
    ["speaker_id", "TEXT"],
    ["speaker_confidence", "REAL"],
  ];
  for (const [name, type] of adds) {
    if (!cols.has(name)) {
      db.exec(`ALTER TABLE audio_chunks ADD COLUMN ${name} ${type}`);
    }
  }
}

function migrateDeviceCols(db) {
  const cols = new Set(db.prepare("PRAGMA table_info(devices)").all().map((c) => c.name));
  if (!cols.has("last_seen_at")) {
    db.exec("ALTER TABLE devices ADD COLUMN last_seen_at TEXT");
  }
  if (!cols.has("node_config_version")) {
    db.exec("ALTER TABLE devices ADD COLUMN node_config_version TEXT");
  }
  if (!cols.has("node_config_applied")) {
    db.exec("ALTER TABLE devices ADD COLUMN node_config_applied TEXT");
  }
}

function migrateMemoryEntityCol(db) {
  if (!tableExists(db, "memory_chunks")) {
    return;
  }
  const cols = new Set(db.prepare("PRAGMA table_info(memory_chunks)").all().map((c) => c.name));
  if (!cols.has("entity_id")) {
    db.exec("ALTER TABLE memory_chunks ADD COLUMN entity_id TEXT");
  }
  db.exec("CREATE INDEX IF NOT EXISTS idx_memory_chunks_entity ON memory_chunks(entity_id)");
}

export function migrateSchemaV2(db) {
  for (const name of LEGACY_TABLES) {
    if (tableExists(db, name)) {
      db.exec(`DROP TABLE IF EXISTS ${name}`);
    }
  }
  migrateDeviceCols(db);
  migrateAudioChunkCols(db);
  migrateMemoryEntityCol(db);
}

export function migrateDb(db) {
  migrateSchemaV2(db);
}

let dbSingleton = null;

export function openDb() {
  if (dbSingleton) {
    return dbSingleton;
  }
  mkdirSync(CFG.dataDir, { recursive: true });
  mkdirSync(PATHS.speakerDir(), { recursive: true });
  const db = new DatabaseSync(PATHS.db());
  db.exec("PRAGMA journal_mode = WAL;");
  db.exec("PRAGMA busy_timeout = 5000;");
  db.exec("PRAGMA foreign_keys = ON;");
  db.exec(DB_SCHEMA);
  migrateDb(db);
  dbSingleton = db;
  return db;
}

// --- devices.mjs ---
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

// --- speakers.mjs ---
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

// --- entities.mjs ---
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

// --- audio.mjs ---
function deletePcmFile(path) {
  if (!path || !existsSync(path)) {
    return;
  }
  try {
    unlinkSync(path);
  } catch {
    /* ignore */
  }
}

export function clearAudioChunkPath(db, id) {
  db.prepare("UPDATE audio_chunks SET path = '' WHERE id = ?").run(id);
}

export function insertAudioChunk(db, row) {
  const info = db
    .prepare(
      `INSERT INTO audio_chunks (device_id, captured_at, seq, path, duration_ms, energy, device_ms)
       VALUES (@device_id, @captured_at, @seq, @path, @duration_ms, @energy, @device_ms)`,
    )
    .run({
      device_ms: null,
      ...row,
    });
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

export function markAudioProcessed(db, id) {
  db.prepare("UPDATE audio_chunks SET processed = 1 WHERE id = ?").run(id);
}

export function updateAudioClassification(db, id, { sound_kind, sound_label, speaker_id, speaker_confidence }) {
  db.prepare(
    `UPDATE audio_chunks SET
      sound_kind = @sound_kind,
      sound_label = @sound_label,
      speaker_id = @speaker_id,
      speaker_confidence = @speaker_confidence
     WHERE id = @id`,
  ).run({
    id,
    sound_kind: sound_kind ?? null,
    sound_label: sound_label ?? null,
    speaker_id: speaker_id ?? null,
    speaker_confidence: speaker_confidence ?? null,
  });
}

export function bumpSttAttempts(db, id) {
  db.prepare("UPDATE audio_chunks SET stt_attempts = stt_attempts + 1 WHERE id = ?").run(id);
  return db.prepare("SELECT stt_attempts FROM audio_chunks WHERE id = ?").get(id)?.stt_attempts ?? 0;
}

export function deleteAudioChunk(db, id) {
  const row = db.prepare("SELECT path FROM audio_chunks WHERE id = ?").get(id);
  deletePcmFile(row?.path);
  db.prepare("DELETE FROM audio_chunks WHERE id = ?").run(id);
}

export function pruneExpiredPcm(db, retentionDays = CFG.audioRetentionDays) {
  if (retentionDays <= 0) {
    return { files: 0 };
  }
  const cutoff = retentionCutoffIso(retentionDays);
  const rows = db
    .prepare(
      `SELECT id, path FROM audio_chunks
       WHERE path != '' AND captured_at < ?`,
    )
    .all(cutoff);
  let files = 0;
  for (const row of rows) {
    deletePcmFile(row.path);
    clearAudioChunkPath(db, row.id);
    files++;
  }
  return { files };
}

// --- utterances.mjs ---
export function insertUtterance(db, row) {
  const info = db
    .prepare(
      `INSERT INTO utterances (chunk_id, speaker_id, started_at, ended_at, text, confidence)
       VALUES (@chunk_id, @speaker_id, @started_at, @ended_at, @text, @confidence)`,
    )
    .run(row);
  return Number(info.lastInsertRowid);
}

export function getAudioChunk(db, id) {
  return db.prepare("SELECT * FROM audio_chunks WHERE id = ?").get(id);
}

export function audioChunksForDay(db, day) {
  const { start, end } = dayBounds(day);
  return db
    .prepare(
      `SELECT c.*, u.id AS utterance_id, u.text AS utterance_text
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

// --- frames.mjs ---
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

// --- events.mjs ---
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

// --- daily.mjs ---
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

// --- memory.mjs ---
export function deleteMemoryForDay(db, day) {
  db.prepare("DELETE FROM memory_chunks WHERE day = ?").run(day);
}

export function insertMemoryChunk(db, row) {
  db.prepare(
    `INSERT INTO memory_chunks (day, kind, text, evidence_json, embedding_json, entity_id, weight, created_at)
     VALUES (@day, @kind, @text, @evidence_json, @embedding_json, @entity_id, @weight, @created_at)`,
  ).run({
    day: row.day,
    kind: row.kind,
    text: row.text,
    evidence_json: row.evidence_json ?? null,
    embedding_json: row.embedding_json ?? null,
    entity_id: row.entity_id ?? null,
    weight: row.weight ?? 1,
    created_at: row.created_at,
  });
}

export function memoryChunksInRange(db, minDay, beforeDay) {
  return db
    .prepare(`SELECT * FROM memory_chunks WHERE day >= ? AND day < ? ORDER BY day DESC`)
    .all(minDay, beforeDay);
}

export function memoryChunkCount(db) {
  return db.prepare("SELECT COUNT(*) AS n FROM memory_chunks").get().n;
}

export function memoryChunksForEntity(db, entityId, limit = 20) {
  return db
    .prepare(
      `SELECT * FROM memory_chunks WHERE entity_id = ? ORDER BY day DESC, id DESC LIMIT ?`,
    )
    .all(entityId, limit);
}

export function profileFactsFromMemory(db, limit = CFG.memoryProfileLimit) {
  return db
    .prepare(
      `SELECT entity_id, kind, text, day FROM memory_chunks
       WHERE entity_id IS NOT NULL
       ORDER BY day DESC, id DESC LIMIT ?`,
    )
    .all(limit);
}

// --- timeline.mjs ---
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

export function timelineForDay(db, day) {
  const audio = audioChunksForDay(db, day).map((c) => ({
    type: "audio",
    at: c.captured_at,
    id: `aud:${c.id}`,
    chunk_id: c.id,
    energy: c.energy,
    speech: (c.energy ?? 0) >= CFG.speechEnergyThreshold,
    sound_kind: c.sound_kind ?? null,
    sound_label: c.sound_label ?? null,
    speaker_id: c.speaker_id ?? null,
    speaker_confidence: c.speaker_confidence ?? null,
    text: c.utterance_text ?? null,
    utterance_id: c.utterance_id ?? null,
    processed: !!c.processed,
    has_pcm: Boolean(c.path),
    device_ms: c.device_ms ?? null,
  }));
  const frames = framesForDay(db, day).map((f) => ({
    type: "frame",
    at: f.captured_at,
    id: `frm:${f.id}`,
    frame_id: f.id,
    caption: f.caption,
    scene_json: f.scene_json ?? null,
    reason: f.reason,
    processed: !!f.processed,
  }));
  return [...audio, ...frames].sort((a, b) => a.at.localeCompare(b.at));
}
