import { CFG } from "../config.mjs";
import {
  entitiesActiveOnDay, getDayInsights, getSpeaker, latestWitnessAt, listEntities, openDb,
  timelineForDay, upsertDayInsights, upsertEntity, witnessStats,
} from "../db/index.mjs";
import { chatJson } from "../llm.mjs";
import { promptLanguageRule } from "../locale.mjs";
import { indexDayMemories, retrieveContextForDay } from "../memory.mjs";
import { isMeaningfulFrameItem } from "../present.mjs";
import { modelId, ModelSlot } from "../models.mjs";
import { dayOffset, isoNow, today, isSttHallucination } from "../util.mjs";

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

function sampleSpread(items, limit) {
  if (items.length <= limit) {
    return items;
  }
  const byHour = {};
  for (const item of items) {
    const hour = item.at?.slice(11, 13) ?? "?";
    (byHour[hour] ??= []).push(item);
  }
  const hours = Object.keys(byHour).sort();
  const out = [];
  let round = 0;
  while (out.length < limit && hours.some((h) => byHour[h]?.length)) {
    const hour = hours[round % hours.length];
    const bucket = byHour[hour];
    if (bucket?.length) {
      out.push(bucket.shift());
    }
    round++;
  }
  return out;
}

function buildRagQuery(stats) {
  const parts = [];
  const soundKeys = Object.keys(stats.sounds ?? {}).slice(0, 4);
  if (soundKeys.length) {
    parts.push(soundKeys.join(" "));
  }
  const speakerNames = Object.values(stats.speakers ?? {}).slice(0, 4);
  if (speakerNames.length) {
    parts.push(speakerNames.join(" "));
  }
  for (const u of stats.sample_utterances ?? []) {
    if (u.text) {
      parts.push(u.text.slice(0, 80));
    }
  }
  for (const f of stats.sample_frames ?? []) {
    if (f.caption) {
      parts.push(f.caption.slice(0, 80));
    }
  }
  const built = parts.join(" ").replace(/\s+/g, " ").trim();
  if (built.length >= 12) {
    return built.slice(0, 512);
  }
  return String(CFG.memoryContextQueryTemplate ?? "").replaceAll("{day}", stats.day).trim();
}

function sampleLimits(stats) {
  const speech = stats.utterances + stats.speech_chunks;
  const scenes = stats.scenes ?? 0;
  return {
    utterances: Math.min(32, Math.max(8, Math.ceil(Math.sqrt(speech + 1) * 4))),
    frames: Math.min(20, Math.max(6, Math.ceil(Math.sqrt(scenes + 1) * 3))),
  };
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

  const utteranceCandidates = items.filter(
    (i) => i.type === "audio" && i.text && !isSttHallucination(i.text, CFG.audioSttRejectPatterns),
  );
  const frameCandidates = items.filter(
    (i) => i.type === "frame" && isMeaningfulFrameItem(i),
  );

  const limits = sampleLimits({ utterances: stats.utterances, speech_chunks: stats.speech, scenes });

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
    sample_utterances: sampleSpread(utteranceCandidates, limits.utterances).map((i) => ({
      at: i.at,
      speaker: i.speaker_id ? speakerNames[i.speaker_id] : null,
      text: i.text.slice(0, 160),
      sound: i.sound_label,
    })),
    sample_frames: sampleSpread(frameCandidates, limits.frames).map((i) => ({
      at: i.at,
      caption: i.caption.slice(0, 120),
    })),
  };
}

async function generateInsightsJson(db, day, stats, ragHits) {
  const ragBlock = ragHits.length
    ? ragHits.map((h) => `- [${h.day}] ${h.text}`).join("\n")
    : "(sem memória anterior relevante)";

  const system =
    "You observe a home passively — like a perceptive friend who lives there. " +
    "Write in natural language, concrete and warm, never like a security report. " +
    "CRITICAL: only describe what appears in the stats JSON or memory block. " +
    "If data is sparse, noisy, or unclear, say so briefly — do NOT invent people, devices, rooms, or events. " +
    "Do NOT speculate about TVs, visitors, or family unless explicitly in the data. " +
    "Return JSON only (no markdown fences). Keep every string on one line. " +
    promptLanguageRule();

  const enrolled = Object.keys(stats.speakers ?? {});
  const speakerHint = enrolled.length
    ? `Enrolled speakers (use speaker:ID only for these): ${enrolled.join(", ")}`
    : "No enrolled speakers — omit entities unless clearly evidenced in sample_utterances or sample_frames.";

  const user =
    `What happened on ${day}:\n${JSON.stringify(stats, null, 2)}\n\n` +
    `${speakerHint}\n\n` +
    `Memory from past days (${ragHits.length} hits):\n${ragBlock}\n\n` +
    "Rules:\n" +
    "- summary: 1-3 sentences grounded in sample_utterances and sample_frames\n" +
    "- moments: max 5, each starts with HH:MM from sample data, one line each\n" +
    "- insights/patterns: optional, only if supported by data; omit if unsure\n" +
    "- entities: max 3, only enrolled speakers or groups with direct evidence; never invent objects\n\n" +
    "Schema: " +
    '{"summary":"…","moments":["15:30 …"],"insights":[],"patterns":[],"entities":[]}';

  return chatJson({
    model: modelId(ModelSlot.DEEP),
    temperature: CFG.insightsTemperature,
    maxTokens: CFG.insightsMaxTokens,
    messages: [
      { role: "system", content: system },
      { role: "user", content: user },
    ],
  }).then((raw) => normalizeInsightsPayload(raw));
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
    const ragQuery = buildRagQuery(stats);
    const ragHits = await retrieveContextForDay(db, day, { query: ragQuery });

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
        rag_hits: ragHits.length,
        rag_query: ragQuery.slice(0, 120),
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

  const latestWitness = latestWitnessAt(db, day);

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
    rag_hits: parsed.rag_hits ?? null,
    entities: JSON.parse(row.entities_json || "[]"),
    updated_at: row.updated_at,
    runtime: insightRuntime(),
  };
}
