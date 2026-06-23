/** SQLite DDL + lightweight migrations (devices columns). */

export const DB_SCHEMA = `
CREATE TABLE IF NOT EXISTS devices (
  id TEXT PRIMARY KEY,
  label TEXT,
  created_at TEXT NOT NULL,
  last_seen_at TEXT,
  node_config_version TEXT,
  node_config_applied TEXT
);

CREATE TABLE IF NOT EXISTS audio_chunks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  device_id TEXT NOT NULL,
  captured_at TEXT NOT NULL,
  seq INTEGER NOT NULL,
  path TEXT NOT NULL,
  duration_ms INTEGER NOT NULL DEFAULT 1000,
  energy REAL,
  processed INTEGER NOT NULL DEFAULT 0,
  FOREIGN KEY (device_id) REFERENCES devices(id)
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

CREATE TABLE IF NOT EXISTS speakers (
  id TEXT PRIMARY KEY,
  display_name TEXT NOT NULL,
  profile_json TEXT,
  embedding_path TEXT,
  created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS episodes (
  id TEXT PRIMARY KEY,
  day TEXT NOT NULL,
  started_at TEXT NOT NULL,
  ended_at TEXT NOT NULL,
  label TEXT,
  summary_json TEXT,
  created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_episodes_day ON episodes(day);

CREATE TABLE IF NOT EXISTS episode_utterances (
  episode_id TEXT NOT NULL,
  utterance_id INTEGER NOT NULL,
  PRIMARY KEY (episode_id, utterance_id),
  FOREIGN KEY (episode_id) REFERENCES episodes(id),
  FOREIGN KEY (utterance_id) REFERENCES utterances(id)
);

CREATE TABLE IF NOT EXISTS episode_frames (
  episode_id TEXT NOT NULL,
  frame_id INTEGER NOT NULL,
  PRIMARY KEY (episode_id, frame_id),
  FOREIGN KEY (episode_id) REFERENCES episodes(id),
  FOREIGN KEY (frame_id) REFERENCES frames(id)
);

CREATE TABLE IF NOT EXISTS graph_nodes (
  id TEXT PRIMARY KEY,
  day TEXT NOT NULL,
  kind TEXT NOT NULL,
  label TEXT NOT NULL,
  payload_json TEXT
);
CREATE INDEX IF NOT EXISTS idx_graph_nodes_day ON graph_nodes(day);

CREATE TABLE IF NOT EXISTS graph_edges (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  day TEXT NOT NULL,
  from_node TEXT NOT NULL,
  to_node TEXT NOT NULL,
  relation TEXT NOT NULL,
  evidence_json TEXT,
  confidence REAL,
  FOREIGN KEY (from_node) REFERENCES graph_nodes(id),
  FOREIGN KEY (to_node) REFERENCES graph_nodes(id)
);
CREATE INDEX IF NOT EXISTS idx_graph_edges_day ON graph_edges(day);

CREATE TABLE IF NOT EXISTS digests (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  day TEXT NOT NULL UNIQUE,
  pass_a_json TEXT,
  pass_b_json TEXT,
  pass_c_json TEXT,
  prose TEXT,
  evidence_json TEXT,
  model_fast TEXT,
  model_deep TEXT,
  created_at TEXT NOT NULL,
  updated_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS profile_facts (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  key TEXT NOT NULL,
  value TEXT NOT NULL,
  source_day TEXT,
  confidence REAL,
  updated_at TEXT NOT NULL
);
CREATE UNIQUE INDEX IF NOT EXISTS idx_profile_facts_key ON profile_facts(key);

CREATE TABLE IF NOT EXISTS day_rollups (
  day TEXT PRIMARY KEY,
  compact_json TEXT NOT NULL,
  created_at TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS memory_chunks (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  day TEXT NOT NULL,
  kind TEXT NOT NULL,
  text TEXT NOT NULL,
  evidence_json TEXT,
  embedding_json TEXT,
  weight REAL NOT NULL DEFAULT 1.0,
  created_at TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_memory_chunks_day ON memory_chunks(day);
CREATE INDEX IF NOT EXISTS idx_memory_chunks_kind ON memory_chunks(kind);
`;

export function migrateDb(db) {
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
