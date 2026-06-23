import { CFG } from "./config.mjs";
import { chatCompletion, chatJson } from "./lm.mjs";
import { isoNow } from "./util.mjs";
import { upsertDayRollup, upsertDigest, upsertProfileFact } from "./db.mjs";
import { rebuildEpisodesForDay } from "./episodes.mjs";
import { buildGraphFromEpisodes } from "./graph.mjs";

function priorContext(db, day) {
  const d = new Date(`${day}T12:00:00.000Z`);
  d.setUTCDate(d.getUTCDate() - 1);
  const priorDay = d.toISOString().slice(0, 10);
  const rollup = db.prepare("SELECT compact_json FROM day_rollups WHERE day = ?").get(priorDay);
  return rollup ? JSON.parse(rollup.compact_json) : null;
}

function profileContext(db) {
  return db.prepare("SELECT key, value, confidence FROM profile_facts ORDER BY key").all();
}

function episodeBundle(db, day) {
  const episodes = db
    .prepare(`SELECT * FROM episodes WHERE day = ? ORDER BY started_at`)
    .all(day);
  return episodes.map((ep) => ({
    id: ep.id,
    label: ep.label,
    started_at: ep.started_at,
    ended_at: ep.ended_at,
    summary: JSON.parse(ep.summary_json || "{}"),
  }));
}

function alignedMoments(db, day) {
  const utterances = db
    .prepare(
      `SELECT id, started_at, text FROM utterances
       WHERE started_at >= ? AND started_at < ? ORDER BY started_at`,
    )
    .all(`${day}T00:00:00.000Z`, `${day}T23:59:59.999Z`)
    .map((u) => ({ id: `utt:${u.id}`, at: u.started_at, text: u.text }));

  const frames = db
    .prepare(
      `SELECT id, captured_at, caption, scene_json FROM frames
       WHERE captured_at >= ? AND captured_at < ? ORDER BY captured_at`,
    )
    .all(`${day}T00:00:00.000Z`, `${day}T23:59:59.999Z`)
    .map((f) => ({
      id: `frm:${f.id}`,
      at: f.captured_at,
      visual: f.caption || f.scene_json,
    }));

  return [...utterances, ...frames].sort((a, b) => a.at.localeCompare(b.at));
}

/** Pass A — factual ledger with evidence IDs only */
export async function passA(db, day) {
  const episodes = episodeBundle(db, day);
  const moments = alignedMoments(db, day);
  const events = db
    .prepare(`SELECT id, at, kind, payload_json FROM events WHERE at >= ? AND at < ?`)
    .all(`${day}T00:00:00.000Z`, `${day}T23:59:59.999Z`);

  return chatJson({
    model: CFG.modelFast,
    messages: [
      {
        role: "system",
        content:
          "You are Pass A of folio digest pipeline. Extract only verifiable facts from witness data. " +
          "Reply raw JSON: {\"timeline\":[{\"at\":\"ISO\",\"fact\":\"\",\"evidence\":[\"utt:N|frm:N|ep:ID\"]}],\"episode_facts\":[{\"episode_id\":\"\",\"facts\":[]}],\"events_notable\":[]}. " +
          "No interpretation. No markdown.",
      },
      {
        role: "user",
        content: JSON.stringify({ day, episodes, moments, events }, null, 2),
      },
    ],
    maxTokens: 3000,
  });
}

/** Pass B — inference, arcs, implicit shifts */
export async function passB(db, day, passAJson, prior) {
  const episodes = episodeBundle(db, day);
  const profile = profileContext(db);

  return chatJson({
    model: CFG.modelDeep,
    messages: [
      {
        role: "system",
        content:
          "You are Pass B (interpretation). Given Pass A facts and episode semantics, infer meaning: emotional arc, " +
          "decisions vs brainstorming, what was abandoned, what remains open, cross-modal alignments (speech + visual timing). " +
          "Reply raw JSON: {\"narrative_arc\":\"\",\"shifts\":[{\"at\":\"\",\"description\":\"\",\"evidence\":[]}],\"decisions_real\":[{\"text\":\"\",\"evidence\":[]}],\"open_loops\":[],\"patterns\":[],\"tomorrow_pull\":[]}. " +
          `Locale ${CFG.defaultLocale}. Write JSON values in Portuguese.`,
      },
      {
        role: "user",
        content: JSON.stringify({ day, pass_a: passAJson, episodes, profile, prior_day: prior }, null, 2),
      },
    ],
    maxTokens: 3500,
  });
}

/** Pass C — adversarial critique: drop unsupported claims */
export async function passC(passAJson, passBJson, moments) {
  return chatJson({
    model: CFG.modelFast,
    messages: [
      {
        role: "system",
        content:
          "You are Pass C (critic). Compare Pass B claims to Pass A facts and raw moments. " +
          "Remove or downgrade any claim lacking evidence. Reply raw JSON: " +
          "{\"approved_claims\":[{\"text\":\"\",\"evidence\":[],\"confidence\":0-1}],\"rejected_claims\":[{\"text\":\"\",\"reason\":\"\"}],\"evidence_gaps\":[]}",
      },
      {
        role: "user",
        content: JSON.stringify({ pass_a: passAJson, pass_b: passBJson, moments }, null, 2),
      },
    ],
    maxTokens: 2500,
  });
}

/** Pass D — final prose letter (no section templates) */
export async function passD(day, passAJson, passBJson, passCJson, prior) {
  const dateLabel = new Date(`${day}T12:00:00.000Z`).toLocaleDateString("pt-BR", {
    weekday: "long",
    day: "numeric",
    month: "long",
  });

  return chatCompletion({
    model: CFG.modelDeep,
    temperature: 0.35,
    maxTokens: 2800,
    messages: [
      {
        role: "system",
        content:
          "You are Pass D (folio chronicler). Write a single intelligent letter about the person's day in Portuguese (Brazil). " +
          "NO markdown headings, NO bullet lists, NO template sections. Write flowing prose paragraphs like a perceptive analyst who watched the whole day. " +
          "Include: arc of the day, what mattered vs noise, cross-modal observations (when speech aligned with what was seen), " +
          "implicit decisions, open threads for tomorrow, patterns vs prior day if provided. " +
          "End with one short italic line: Evidência: comma-separated evidence IDs used. " +
          "Only include claims approved in Pass C.",
      },
      {
        role: "user",
        content: JSON.stringify(
          {
            title: `Folio · ${dateLabel}`,
            pass_a: passAJson,
            pass_b: passBJson,
            pass_c: passCJson,
            prior_day: prior,
          },
          null,
          2,
        ),
      },
    ],
  });
}

export async function runDigestPipeline(db, day) {
  console.log(`[digest] rebuilding episodes for ${day}`);
  const episodes = await rebuildEpisodesForDay(db, day);
  buildGraphFromEpisodes(db, day, episodes);

  const prior = priorContext(db, day);
  console.log("[digest] pass A — facts");
  const a = await passA(db, day);
  console.log("[digest] pass B — interpretation");
  const b = await passB(db, day, a, prior);
  const moments = alignedMoments(db, day);
  console.log("[digest] pass C — critique");
  const c = await passC(a, b, moments);
  console.log("[digest] pass D — prose");
  const prose = await passD(day, a, b, c, prior);

  const evidence = {
    approved: c.approved_claims ?? [],
    gaps: c.evidence_gaps ?? [],
  };

  const now = isoNow();
  upsertDigest(db, {
    day,
    pass_a_json: JSON.stringify(a),
    pass_b_json: JSON.stringify(b),
    pass_c_json: JSON.stringify(c),
    prose,
    evidence_json: JSON.stringify(evidence),
    model_fast: CFG.modelFast,
    model_deep: CFG.modelDeep,
    created_at: now,
    updated_at: now,
  });

  const compact = {
    day,
    arc: b.narrative_arc,
    decisions: b.decisions_real,
    open_loops: b.open_loops,
    tomorrow_pull: b.tomorrow_pull,
    patterns: b.patterns,
  };
  upsertDayRollup(db, day, JSON.stringify(compact));

  for (const p of b.patterns ?? []) {
    if (typeof p === "string" && p.length > 4) {
      upsertProfileFact(db, `pattern:${p.slice(0, 48)}`, p, day, 0.6);
    }
  }

  return { day, prose, passes: { a, b, c } };
}
