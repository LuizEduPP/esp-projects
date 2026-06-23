#!/usr/bin/env node
/**
 * folio-enroll — register a speaker profile (metadata only; embedding optional later).
 * Usage: node folio-enroll.mjs <speaker_id> <display_name>
 */
import { openDb } from "./lib/db.mjs";
import { isoNow } from "./lib/util.mjs";

function main() {
  const speakerId = process.argv[2];
  const displayName = process.argv[3];
  if (!speakerId || !displayName) {
    console.error("Usage: folio-enroll.mjs <speaker_id> <display_name>");
    process.exit(1);
  }

  const db = openDb();
  db.prepare(
    `INSERT OR REPLACE INTO speakers (id, display_name, profile_json, embedding_path, created_at)
     VALUES (?, ?, ?, ?, ?)`,
  ).run(speakerId, displayName, JSON.stringify({ locale: "pt-BR" }), null, isoNow());

  console.log(`Speaker enrolled: ${speakerId} (${displayName})`);
  console.log("Voice embedding: not implemented — diarization uses STT text only for now.");
}

main();
