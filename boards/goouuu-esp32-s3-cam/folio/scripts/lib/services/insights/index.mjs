import { CFG } from "../../config/index.mjs";
import { chatJson } from "../../llm/client.mjs";
import { promptLanguageRule } from "../../locale/index.mjs";
import { modelId, ModelSlot } from "../../models/index.mjs";
import {
  entitiesActiveOnDay,
  getDayInsights,
  getSpeaker,
  openDb,
  timelineForDay,
  upsertDayInsights,
  upsertEntity,
  witnessStats,
} from "../../db/index.mjs";
import { indexDayMemories, retrieveContextForDay } from "../../memory/index.mjs";
import { isoNow, today } from "../../util.mjs";

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
    utterances: stats.utterances,
    speech_chunks: stats.speech,
    sounds,
    speakers: speakerNames,
    speaker_counts: speakers,
    activity_by_hour: hours,
    sample_utterances: items
      .filter((i) => i.type === "audio" && i.text)
      .slice(0, 24)
      .map((i) => ({
        at: i.at,
        speaker: i.speaker_id ? speakerNames[i.speaker_id] : null,
        text: i.text.slice(0, 160),
        sound: i.sound_label,
      })),
    sample_frames: items
      .filter((i) => i.type === "frame" && i.caption)
      .slice(0, 12)
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
    "Return JSON only (no markdown). " +
    promptLanguageRule();

  const user =
    `What happened on ${day}:\n${JSON.stringify(stats, null, 2)}\n\n` +
    `Memory from past days:\n${ragBlock}\n\n` +
    "Schema: " +
    '{"summary":"2-4 sentences — what the day felt like, who was around, notable moments",' +
    '"moments":["short highlights with time when known, e.g. 14:30 alguém falou sobre…"],' +
    '"insights":["optional deeper observations"],' +
    '"patterns":["recurring habits only if evidence exists"],' +
    '"entities":[{"id":"speaker:xyz|pet:dog|…","name":"…","kind":"person|pet|other","notes":"…"}]}';

  return chatJson({
    model: modelId(ModelSlot.DEEP),
    temperature: CFG.insightsTemperature,
    maxTokens: 2048,
    messages: [
      { role: "system", content: system },
      { role: "user", content: user },
    ],
  });
}

function syncEntitiesFromInsights(db, day, entities = []) {
  for (const ent of entities) {
    if (!ent?.id || !ent?.name) {
      continue;
    }
    upsertEntity(db, {
      id: ent.id,
      kind: ent.kind || "other",
      display_name: ent.name,
      speaker_id: ent.kind === "person" && ent.id.startsWith("speaker:") ? ent.id.slice(8) : null,
      profile_json: JSON.stringify({ notes: ent.notes ?? "" }),
      patterns_json: JSON.stringify({ last_seen: day, notes: ent.notes ?? "" }),
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

  if ((stats.sounds?.bark ?? 0) > 0) {
    upsertEntity(db, {
      id: "pet:dog",
      kind: "pet",
      display_name: "Cachorro",
      speaker_id: null,
      profile_json: JSON.stringify({ kind: "dog" }),
      patterns_json: JSON.stringify({
        last_seen: day,
        barks_today: stats.sounds.bark,
      }),
    });
  }
}

export async function runDayInsights(db, day, { force = false } = {}) {
  const stats = buildDayStats(db, day);
  const hasData = stats.frames + stats.utterances + stats.speech_chunks > 0;
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
    const insightsPayload = await generateInsightsJson(db, day, stats, ragHits);
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
  const hasData = stats.frames + stats.utterances + stats.speech > 0;
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
