import { CFG } from "../config.mjs";
import { chatCompletion, chatJson } from "../lm.mjs";
import { dateLabelForDay, evidenceFooterRule, promptLanguageRule } from "../locale.mjs";
import { isoNow } from "../util.mjs";
import {
  alignedMomentsForDay,
  episodeSummariesForDay,
  eventsForDay,
  priorDayRollup,
  profileFacts,
  upsertDayRollup,
  upsertDigest,
  upsertProfileFact,
} from "../db.mjs";
import { rebuildEpisodesForDay } from "../episodes.mjs";
import { buildGraphFromEpisodes } from "../graph.mjs";

export async function passA(db, day) {
  const episodes = episodeSummariesForDay(db, day);
  const moments = alignedMomentsForDay(db, day);
  const events = eventsForDay(db, day);

  return chatJson({
    model: CFG.modelFast,
    messages: [
      {
        role: "system",
        content:
          "You are Pass A of folio digest pipeline. Extract only verifiable facts from witness data. " +
          'Reply raw JSON: {"timeline":[{"at":"ISO","fact":"","evidence":["utt:N|frm:N|ep:ID"]}],' +
          '"episode_facts":[{"episode_id":"","facts":[]}],"events_notable":[]}. ' +
          "No interpretation. No markdown. " +
          promptLanguageRule(),
      },
      {
        role: "user",
        content: JSON.stringify({ day, episodes, moments, events }, null, 2),
      },
    ],
    maxTokens: 3000,
  });
}

export async function passB(db, day, passAJson, prior) {
  const episodes = episodeSummariesForDay(db, day);
  const profile = profileFacts(db);

  return chatJson({
    model: CFG.modelDeep,
    messages: [
      {
        role: "system",
        content:
          "You are Pass B (interpretation). Given Pass A facts and episode semantics, infer meaning: emotional arc, " +
          "decisions vs brainstorming, what was abandoned, what remains open, cross-modal alignments (speech + visual timing). " +
          'Reply raw JSON: {"narrative_arc":"","shifts":[{"at":"","description":"","evidence":[]}],' +
          '"decisions_real":[{"text":"","evidence":[]}],"open_loops":[],"patterns":[],"tomorrow_pull":[]}. ' +
          promptLanguageRule(),
      },
      {
        role: "user",
        content: JSON.stringify({ day, pass_a: passAJson, episodes, profile, prior_day: prior }, null, 2),
      },
    ],
    maxTokens: 3500,
  });
}

export async function passC(passAJson, passBJson, moments) {
  return chatJson({
    model: CFG.modelFast,
    messages: [
      {
        role: "system",
        content:
          "You are Pass C (critic). Compare Pass B claims to Pass A facts and raw moments. " +
          "Remove or downgrade any claim lacking evidence. Reply raw JSON: " +
          '{"approved_claims":[{"text":"","evidence":[],"confidence":0-1}],' +
          '"rejected_claims":[{"text":"","reason":""}],"evidence_gaps":[]}. ' +
          promptLanguageRule(),
      },
      {
        role: "user",
        content: JSON.stringify({ pass_a: passAJson, pass_b: passBJson, moments }, null, 2),
      },
    ],
    maxTokens: 2500,
  });
}

export async function passD(day, passAJson, passBJson, passCJson, prior) {
  const dateLabel = dateLabelForDay(day);

  return chatCompletion({
    model: CFG.modelDeep,
    temperature: 0.35,
    maxTokens: 2800,
    messages: [
      {
        role: "system",
        content:
          "You are Pass D (folio chronicler). Write a single intelligent letter about the person's day. " +
          promptLanguageRule() +
          " NO markdown headings, NO bullet lists, NO template sections. Write flowing prose paragraphs like a perceptive analyst who watched the whole day. " +
          "Include: arc of the day, what mattered vs noise, cross-modal observations (when speech aligned with what was seen), " +
          "implicit decisions, open threads for tomorrow, patterns vs prior day if provided. " +
          evidenceFooterRule() +
          " Only include claims approved in Pass C.",
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

  const prior = priorDayRollup(db, day);
  console.log("[digest] pass A — facts");
  const a = await passA(db, day);
  console.log("[digest] pass B — interpretation");
  const b = await passB(db, day, a, prior);
  const moments = alignedMomentsForDay(db, day);
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
