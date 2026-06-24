import { existsSync, mkdirSync, unlinkSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "./config.mjs";
import {
  alignedMomentsForDay, bumpSttAttempts, deleteAudioChunk, ensureDevice, entitiesActiveOnDay,
  getDayInsights, getSpeaker, insertAudioChunk, insertEvent, insertFrame, insertUtterance,
  latestWitnessAt, listEntities, openDb, pendingAudioChunks, pendingCounts, pendingFrames, pruneExpiredPcm,
  timelineForDay, touchDevice, upsertDayInsights, upsertEntity, witnessStats,
} from "./db.mjs";
import { chatJsonLenient } from "./llm.mjs";
import { promptLanguageRule } from "./locale.mjs";
import { indexDayMemories, retrieveContextForDay, retrieveMemories } from "./memory.mjs";
import { isMeaningfulFrameItem } from "./present.mjs";
import { modelId, ModelSlot } from "./models.mjs";
import { processAudioChunk, processFrame } from "./perception/index.mjs";
import { isSpeechChunk, pcmEnergy, shouldStoreAudioChunk } from "./stt.mjs";
import { dayFromIso, dayOffset, isoNow, parseInsightsFallback, parseJsonLoose, parseMetaHeader, today, isSttHallucination } from "./util.mjs";


// --- ingest/index.mjs ---
export function ingestAudioChunk(deviceId, pcmBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const seq = Number(meta.seq ?? 0);
  const deviceMs = meta.ts_ms != null ? Number(meta.ts_ms) : null;

  const metaEnergy = meta.energy != null ? Number(meta.energy) : null;
  const energy = Number.isFinite(metaEnergy) ? metaEnergy : pcmEnergy(pcmBuffer);
  const gate = shouldStoreAudioChunk(pcmBuffer, energy);

  if (!gate.store) {
    return { id: null, energy: gate.energy, speech: false, skipped: gate.reason };
  }

  const dir = PATHS.audioDir(day);
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `${deviceId}-${seq}-${Date.now()}.pcm`);
  writeFileSync(path, pcmBuffer);

  const id = insertAudioChunk(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    seq,
    path,
    duration_ms: CFG.audioChunkMs,
    energy,
    device_ms: deviceMs,
  });

  if (isSpeechChunk(energy)) {
    insertEvent(db, {
      device_id: deviceId,
      at: capturedAt,
      kind: "presence",
      payload_json: JSON.stringify({
        source: "audio",
        energy,
        seq,
        chunk_id: id,
        device_ms: deviceMs,
      }),
    });
  }

  return { id, energy, speech: isSpeechChunk(energy), device_ms: deviceMs };
}

export function ingestFrame(deviceId, jpegBuffer, metaHeader) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const meta = parseMetaHeader(metaHeader);
  const capturedAt = isoNow();
  const day = dayFromIso(capturedAt);
  const dir = PATHS.frameDir(day);
  mkdirSync(dir, { recursive: true });
  const path = join(dir, `${deviceId}-${Date.now()}.jpg`);
  writeFileSync(path, jpegBuffer);

  const id = insertFrame(db, {
    device_id: deviceId,
    captured_at: capturedAt,
    path,
    reason: meta.reason ?? "unknown",
  });

  insertEvent(db, {
    device_id: deviceId,
    at: capturedAt,
    kind: "frame",
    payload_json: JSON.stringify({
      frame_id: id,
      reason: meta.reason ?? "unknown",
      bytes: jpegBuffer.length,
      device_ms: meta.ts_ms != null ? Number(meta.ts_ms) : null,
      pending: true,
    }),
  });

  return { id, reason: meta.reason };
}

export function ingestEvent(deviceId, body) {
  const db = openDb();
  ensureDevice(db, deviceId);
  const kind = String(body.kind ?? "unknown");
  insertEvent(db, {
    device_id: deviceId,
    at: isoNow(),
    kind,
    payload_json: JSON.stringify(body.payload ?? {}),
  });
  return { ok: true };
}

// --- pipeline/index.mjs ---
let lastFrameLmAt = 0;
let loggedWhisperMissing = false;

/** Shorter gap when frames pile up — catch up without hammering LM at steady state. */
function frameCaptionGapMs(pendingFrameCount) {
  const base = CFG.frameCaptionIntervalMs;
  const g = CFG.frameBacklogGap ?? {};
  if (pendingFrameCount > (g.thresholdHigh ?? 20)) {
    return Math.min(base, g.gapHigh ?? 3000);
  }
  if (pendingFrameCount > (g.thresholdMedium ?? 10)) {
    return Math.min(base, g.gapMedium ?? 8000);
  }
  if (pendingFrameCount > (g.thresholdLow ?? 5)) {
    return Math.min(base, g.gapLow ?? 15000);
  }
  return base;
}

function deleteChunkFile(path) {
  if (!path || !existsSync(path)) {
    return;
  }
  try {
    unlinkSync(path);
  } catch {
    /* ignore */
  }
}

export function discardAudioChunk(db, chunk) {
  deleteChunkFile(chunk.path);
  deleteAudioChunk(db, chunk.id);
}

function deleteChunkRows(db, rows) {
  if (!rows.length) {
    return 0;
  }
  for (const row of rows) {
    deleteChunkFile(row.path);
  }
  const batchSize = 400;
  for (let i = 0; i < rows.length; i += batchSize) {
    const batch = rows.slice(i, i + batchSize);
    const placeholders = batch.map(() => "?").join(",");
    db.prepare(`DELETE FROM audio_chunks WHERE id IN (${placeholders})`).run(
      ...batch.map((r) => r.id),
    );
  }
  return rows.length;
}

export function pruneStaleAudio(db = openDb()) {
  const quiet = db
    .prepare(
      `SELECT id, path FROM audio_chunks
       WHERE energy < ? AND (
         processed = 0
         OR (processed = 1 AND path IS NOT NULL AND path != '')
       )`,
    )
    .all(CFG.speechEnergyThreshold);

  const orphan = db
    .prepare(
      `SELECT c.id, c.path FROM audio_chunks c
       LEFT JOIN utterances u ON u.chunk_id = c.id
       WHERE c.processed = 1 AND u.id IS NULL AND c.path != ''
         AND c.energy >= ?`,
    )
    .all(CFG.speechEnergyThreshold);

  const seen = new Set();
  const rows = [];
  for (const row of [...quiet, ...orphan]) {
    if (!seen.has(row.id)) {
      seen.add(row.id);
      rows.push(row);
    }
  }

  return { files: deleteChunkRows(db, rows) };
}

export function runRetentionPass() {
  const db = openDb();
  const stale = pruneStaleAudio(db);
  const expired = pruneExpiredPcm(db, CFG.audioRetentionDays);
  const total = stale.files + expired.files;
  if (total > 0) {
    console.log(
      `[retention] removed ${total} PCM file(s) ` +
        `(stale=${stale.files} expired>${CFG.audioRetentionDays}d=${expired.files}; transcripts kept)`,
    );
  }
  return { files: total, stale: stale.files, expired: expired.files };
}

export function startRetentionLoop() {
  runRetentionPass();
  setInterval(runRetentionPass, CFG.audioRetentionSweepMs);
}

function isWhisperError(err) {
  const msg = String(err?.message ?? err ?? "");
  return err?.code === "ENOENT" || /whisper/i.test(msg);
}

export async function processPendingAudio(limit = CFG.pipelineAudioBatch) {
  const db = openDb();
  const pending = pendingCounts(db);
  if (pending.audio > CFG.workerBacklogHigh) {
    limit = Math.min(CFG.workerBatchMaxHigh, limit * 3);
  } else if (pending.audio > CFG.workerBacklogMedium) {
    limit = Math.min(CFG.workerBatchMaxMedium, limit * 2);
  }

  const chunks = pendingAudioChunks(db, limit);
  const results = [];

  for (const chunk of chunks) {
    try {
      results.push(await processAudioChunk(db, chunk));
    } catch (err) {
      results.push({ id: chunk.id, error: err.message });
    }
  }

  return results;
}

export async function processPendingFrames(limit = CFG.pipelineFrameBatch, { bypassGap = false } = {}) {
  const db = openDb();
  const pending = pendingCounts(db);
  if (pending.frames > CFG.workerBacklogHigh) {
    limit = Math.max(CFG.workerFrameSkipBatch ?? 32, limit);
  } else if (pending.frames > CFG.workerBacklogMedium) {
    limit = Math.min(CFG.workerBatchMaxMedium, limit * 2);
  }

  const minGap = bypassGap ? 0 : frameCaptionGapMs(pending.frames);
  if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
    return [];
  }

  const frames = pendingFrames(db, limit);
  const results = [];

  for (const frame of frames) {
    if (minGap > 0 && Date.now() - lastFrameLmAt < minGap) {
      break;
    }
    try {
      const result = await processFrame(db, frame);
      results.push(result);
      if (result.usedLm) {
        lastFrameLmAt = Date.now();
        if (minGap > 0) {
          break;
        }
      }
    } catch (err) {
      results.push({ id: frame.id, error: err.message, usedLm: false });
      console.warn(`[perception] frame ${frame.id}: ${err.message}`);
    }
  }

  return results;
}

export async function runPendingQueueOnce({ bypassFrameGap = false } = {}) {
  const audio = await processPendingAudio();
  const frames = await processPendingFrames(CFG.pipelineFrameBatch, { bypassGap: bypassFrameGap });
  return { audio, frames };
}

export function startProcessingLoop(intervalMs = CFG.pipelineIntervalMs) {
  let busy = false;

  console.log(
    `[worker] every ${intervalMs}ms · audio batch=${CFG.pipelineAudioBatch} · ` +
      `frame batch=${CFG.pipelineFrameBatch} · LM gap=${CFG.frameCaptionIntervalMs}ms`,
  );

  return setInterval(async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const db = openDb();
      const pending = pendingCounts(db);
      if (pending.audio === 0 && pending.frames === 0) {
        return;
      }

      if (pending.audio > 0) {
        console.log(`[whisper] queue ${pending.audio} audio chunk(s)`);
      }
      if (pending.frames > 5) {
        console.log(`[worker] frame backlog ${pending.frames} — faster caption gap`);
      }

      const audio = await processPendingAudio();
      const frames = await processPendingFrames();

      const whisperErrors = audio.filter((r) => r.error && isWhisperError({ message: r.error }));
      if (whisperErrors.length && !loggedWhisperMissing) {
        loggedWhisperMissing = true;
        console.error(
          "[whisper] CLI not available — pip install openai-whisper or set audio.whisperBin in config",
        );
      }

      const utt = audio.filter((r) => r.text).length;
      const cap = frames.filter((r) => r.caption).length;
      const skipped = frames.filter((r) => r.skipped).length;
      const snd = audio.filter((r) => r.sound).length;
      if (utt > 0 || cap > 0 || snd > 0 || skipped > 0) {
        console.log(`[worker] done utt=${utt} caption=${cap} skip=${skipped} sounds=${snd}`);
      }
    } catch (err) {
      console.error(`[worker] ${err.message}`);
    } finally {
      busy = false;
    }
  }, intervalMs);
}

// --- insights/index.mjs ---
const insightsRuntime = { busy: false, phase: null, error: null, day: null };

export function insightRuntime() {
  return { ...insightsRuntime };
}

function setPhase(day, phase) {
  insightsRuntime.day = day;
  insightsRuntime.phase = phase;
  insightsRuntime.error = null;
}

function setError(day, err) {
  insightsRuntime.busy = false;
  insightsRuntime.phase = "error";
  insightsRuntime.error = err?.message ?? String(err);
  insightsRuntime.day = day;
}

export function buildDayStats(db, day) {
  const items = timelineForDay(db, day);
  const stats = witnessStats(db, day);
  const scenes = items.filter(isMeaningfulFrameItem).length;
  const sounds = {};
  const speakers = {};
  const hours = {};

  for (const item of items) {
    const hour = item.at?.slice(11, 13) ?? "?";
    hours[hour] = (hours[hour] ?? 0) + 1;

    if (item.type === "audio") {
      if (item.sound_kind) {
        sounds[item.sound_kind] = (sounds[item.sound_kind] ?? 0) + 1;
      }
      if (item.speaker_id) {
        speakers[item.speaker_id] = (speakers[item.speaker_id] ?? 0) + 1;
      }
    }
  }

  const speakerNames = {};
  for (const [id, count] of Object.entries(speakers)) {
    speakerNames[id] = getSpeaker(db, id)?.display_name ?? id;
  }

  return {
    day,
    frames: stats.frames,
    scenes,
    utterances: stats.utterances,
    speech_chunks: stats.speech,
    sounds,
    speakers: speakerNames,
    speaker_counts: speakers,
    activity_by_hour: hours,
    sample_utterances: items
      .filter((i) => i.type === "audio" && i.text && !isSttHallucination(i.text, CFG.audioSttRejectPatterns))
      .slice(0, CFG.insightsSampleUtterances)
      .map((i) => ({
        at: i.at,
        speaker: i.speaker_id ? speakerNames[i.speaker_id] : null,
        text: i.text.slice(0, 160),
        sound: i.sound_label,
      })),
    sample_frames: items
      .filter((i) => i.type === "frame" && i.caption)
      .slice(0, CFG.insightsSampleFrames)
      .map((i) => ({ at: i.at, caption: i.caption.slice(0, 120) })),
  };
}

async function generateInsightsJson(db, day, stats, ragHits) {
  const ragBlock = ragHits.length
    ? ragHits.map((h) => `- [${h.day}] ${h.text}`).join("\n")
    : "(sem memória anterior relevante)";

  const system =
    "You observe a home passively — like a perceptive friend who lives there. " +
    "Write in natural language, concrete and warm, never like a security report or JSON template. " +
    "Return JSON only (no markdown fences). Keep every string on one line — no raw line breaks inside values. " +
    promptLanguageRule();

  const enrolled = Object.keys(stats.speakers ?? {});
  const speakerHint = enrolled.length
    ? `Enrolled speakers (use speaker:ID only for these): ${enrolled.join(", ")}`
    : "No enrolled speakers — use group:slug for people/groups and other:slug for objects; never invent speaker: IDs.";

  const user =
    `What happened on ${day}:\n${JSON.stringify(stats, null, 2)}\n\n` +
    `${speakerHint}\n\n` +
    `Memory from past days:\n${ragBlock}\n\n` +
    "Schema: " +
    '{"summary":"2-4 sentences, one paragraph",' +
    '"moments":["15:30 highlight as plain string, …"],' +
    '"insights":["optional short observations as strings"],' +
    '"patterns":["habit as plain string with brief evidence"],' +
    '"entities":[{"id":"group:familia|other:tv|speaker:ENROLLED_ID","name":"short label","kind":"group|person|pet|other","notes":"max 120 chars"}]}';

  return chatJsonLenient({
    model: modelId(ModelSlot.DEEP),
    temperature: CFG.insightsTemperature,
    maxTokens: CFG.insightsMaxTokens,
    messages: [
      { role: "system", content: system },
      { role: "user", content: user },
    ],
    fallback: (text) =>
      normalizeInsightsPayload(parseJsonLoose(text) ?? parseInsightsFallback(text)),
  });
}

function formatInsightItem(value) {
  if (value == null || value === "") {
    return "";
  }
  if (typeof value === "string") {
    return value.trim();
  }
  if (typeof value === "object") {
    if (value.description && value.time) {
      return `${value.time} — ${value.description}`.trim();
    }
    if (value.description) {
      return String(value.description).trim();
    }
    if (value.pattern) {
      const ev = value.evidence ? ` (${value.evidence})` : "";
      return `${value.pattern}${ev}`.trim();
    }
    if (value.text) {
      return String(value.text).trim();
    }
  }
  return String(value).trim();
}

function asInsightList(value) {
  if (Array.isArray(value)) {
    return value.map(formatInsightItem).filter(Boolean);
  }
  const one = formatInsightItem(value);
  return one ? [one] : [];
}

function asEntityArray(value) {
  if (Array.isArray(value)) {
    return value.filter((e) => e && typeof e === "object");
  }
  if (value && typeof value === "object") {
    return [value];
  }
  return [];
}

function normalizeInsightsPayload(raw) {
  if (!raw || typeof raw !== "object") {
    return { summary: "", moments: [], insights: [], patterns: [], entities: [] };
  }

  // Model sometimes returns a single entity object instead of the full schema.
  if (raw.id && raw.name && raw.summary == null && raw.moments == null && raw.insights == null) {
    return {
      summary: String(raw.notes ?? raw.name ?? "").trim(),
      moments: [],
      insights: [],
      patterns: [],
      entities: [raw],
    };
  }

  return {
    summary: String(raw.summary ?? "").trim(),
    moments: asInsightList(raw.moments),
    insights: asInsightList(raw.insights),
    patterns: asInsightList(raw.patterns),
    entities: asEntityArray(raw.entities),
  };
}

function slugFromName(name) {
  return String(name ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^a-z0-9]+/g, "-")
    .replace(/^-+|-+$/g, "")
    .slice(0, 48) || "item";
}

function normalizeInsightEntity(db, ent) {
  const name = String(ent.name ?? "").trim();
  if (!name) {
    return null;
  }
  const notes = String(ent.notes ?? "").trim().slice(0, 120);
  let kind = String(ent.kind ?? "other").toLowerCase();
  const rawId = String(ent.id ?? "").trim();
  const slug = rawId.includes(":") ? rawId.split(":").slice(1).join(":") : slugFromName(name);

  let entityId = rawId || `other:${slug}`;
  let speakerId = null;

  if (rawId.startsWith("speaker:")) {
    const sid = rawId.slice(8).trim();
    if (sid && getSpeaker(db, sid)) {
      speakerId = sid;
      entityId = `speaker:${sid}`;
      kind = "person";
    } else {
      entityId = `group:${slug || sid}`;
      kind = "group";
    }
  } else if (!rawId.includes(":")) {
    const prefix = kind === "pet" ? "pet" : kind.includes("person") ? "group" : "other";
    entityId = `${prefix}:${slug}`;
    if (kind.includes("person")) {
      kind = "group";
    }
  }

  return { id: entityId, kind, display_name: name, speaker_id: speakerId, notes };
}

function purgeInventedSpeakerEntities(db) {
  for (const e of listEntities(db)) {
    if (!e.id.startsWith("speaker:")) {
      continue;
    }
    const sid = e.id.slice(8);
    if (!getSpeaker(db, sid)) {
      db.prepare("DELETE FROM entities WHERE id = ?").run(e.id);
    }
  }
}

function syncEntitiesFromInsights(db, day, entities = []) {
  purgeInventedSpeakerEntities(db);
  for (const ent of asEntityArray(entities)) {
    const row = normalizeInsightEntity(db, ent);
    if (!row) {
      continue;
    }
    upsertEntity(db, {
      id: row.id,
      kind: row.kind,
      display_name: row.display_name,
      speaker_id: row.speaker_id,
      profile_json: JSON.stringify({ notes: row.notes }),
      patterns_json: JSON.stringify({ last_seen: day, notes: row.notes }),
    });
  }
}

function updatePatternsFromStats(db, day, stats) {
  for (const [speakerId, count] of Object.entries(stats.speaker_counts ?? {})) {
    const entityId = `speaker:${speakerId}`;
    const name = stats.speakers[speakerId] ?? speakerId;
    upsertEntity(db, {
      id: entityId,
      kind: "person",
      display_name: name,
      speaker_id: speakerId,
      profile_json: JSON.stringify({ speaker_id: speakerId }),
      patterns_json: JSON.stringify({
        last_seen: day,
        utterances_today: count,
      }),
    });
  }
}

export async function runDayInsights(db, day, { force = false } = {}) {
  const stats = buildDayStats(db, day);
  const hasData = (stats.scenes ?? 0) + stats.utterances + stats.speech_chunks > 0;
  if (!hasData && !force) {
    return { skipped: true, reason: "no_witness" };
  }

  insightsRuntime.busy = true;
  setPhase(day, "index");

  try {
    await indexDayMemories(db, day);

    setPhase(day, "rag");
    const ragHits = await retrieveContextForDay(db, day);

    setPhase(day, "insights");
    const insightsRaw = await generateInsightsJson(db, day, stats, ragHits);
    const insightsPayload = normalizeInsightsPayload(insightsRaw);
    const now = isoNow();

    updatePatternsFromStats(db, day, stats);
    syncEntitiesFromInsights(db, day, insightsPayload.entities ?? []);

    upsertDayInsights(db, {
      day,
      stats_json: JSON.stringify(stats),
      insights_json: JSON.stringify({
        summary: insightsPayload.summary ?? "",
        moments: insightsPayload.moments ?? [],
        insights: insightsPayload.insights ?? [],
        patterns: insightsPayload.patterns ?? [],
      }),
      entities_json: JSON.stringify(entitiesActiveOnDay(db, day)),
      model: modelId(ModelSlot.DEEP),
      created_at: now,
      updated_at: now,
    });

    insightsRuntime.busy = false;
    insightsRuntime.phase = "done";
    insightsRuntime.error = null;

    console.log(`[insights] ${day} — ${insightsPayload.insights?.length ?? 0} insights`);
    return { day, stats, insights: insightsPayload };
  } catch (err) {
    setError(day, err);
    throw err;
  }
}

export function needsInsightsRefresh(db, day) {
  const stats = witnessStats(db, day);
  const items = timelineForDay(db, day);
  const scenes = items.filter(isMeaningfulFrameItem).length;
  const hasData = scenes + stats.utterances + stats.speech > 0;
  if (!hasData) {
    return { needed: false, reason: "no_witness", stats };
  }

  const row = getDayInsights(db, day);
  if (!row) {
    return { needed: true, reason: "missing", stats };
  }

  const latestWitness = db
    .prepare(
      `SELECT MAX(t) AS m FROM (
         SELECT MAX(captured_at) AS t FROM audio_chunks WHERE substr(captured_at,1,10)=?
         UNION ALL SELECT MAX(captured_at) FROM frames WHERE substr(captured_at,1,10)=?
         UNION ALL SELECT MAX(started_at) FROM utterances WHERE substr(started_at,1,10)=?
       )`,
    )
    .get(day, day, day)?.m;

  if (latestWitness && row.updated_at && latestWitness > row.updated_at) {
    return { needed: true, reason: "stale_witness", stats, latestWitness };
  }

  return { needed: false, reason: "up_to_date", stats, updated_at: row.updated_at };
}

export function startInsightsLoop(intervalMs = CFG.insightsIntervalMs) {
  console.log(`[insights] auto every ${intervalMs}ms`);

  return setInterval(async () => {
    if (insightsRuntime.busy) {
      return;
    }
    try {
      const db = openDb();
      const day = today();
      const check = needsInsightsRefresh(db, day);
      if (!check.needed) {
        return;
      }
      await runDayInsights(db, day);
    } catch (err) {
      console.error(`[insights] ${err.message}`);
    }
  }, intervalMs);
}

export function getInsightsForApi(db, day) {
  const row = getDayInsights(db, day);
  if (!row) {
    return {
      day,
      stats: null,
      summary: null,
      moments: [],
      insights: [],
      patterns: [],
      entities: [],
      updated_at: null,
    };
  }
  const parsed = JSON.parse(row.insights_json || "{}");
  return {
    day,
    stats: JSON.parse(row.stats_json || "{}"),
    summary: parsed.summary ?? null,
    moments: parsed.moments ?? [],
    insights: parsed.insights ?? [],
    patterns: parsed.patterns ?? [],
    entities: JSON.parse(row.entities_json || "[]"),
    updated_at: row.updated_at,
    runtime: insightRuntime(),
  };
}
