import { DatabaseSync } from "node:sqlite";
import { mkdirSync } from "node:fs";
import { CFG, PATHS } from "../config/index.mjs";
import { DB_SCHEMA, LEGACY_TABLES } from "./schema.mjs";

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
