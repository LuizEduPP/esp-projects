
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
