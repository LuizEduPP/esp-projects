import { DatabaseSync } from "node:sqlite";
import { mkdirSync } from "node:fs";
import { CFG, PATHS } from "../config/index.mjs";
import { DB_SCHEMA } from "./schema.mjs";

function migrateAudioChunkCols(db) {
  const cols = new Set(db.prepare("PRAGMA table_info(audio_chunks)").all().map((c) => c.name));
  if (!cols.has("device_ms")) {
    db.exec("ALTER TABLE audio_chunks ADD COLUMN device_ms INTEGER");
  }
  if (!cols.has("stt_attempts")) {
    db.exec("ALTER TABLE audio_chunks ADD COLUMN stt_attempts INTEGER NOT NULL DEFAULT 0");
  }
}

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
  migrateAudioChunkCols(db);
}

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
  db.exec("PRAGMA foreign_keys = ON;");
  db.exec(DB_SCHEMA);
  migrateDb(db);
  dbSingleton = db;
  return db;
}
