import { CFG } from "../config.mjs";
import { chatCompletion, chatJson } from "../lm.mjs";
import { dateLabelForDay, evidenceFooterRule, promptLanguageRule } from "../locale.mjs";
import { isoNow } from "../util.mjs";
import {
  alignedMomentsForDay,
  episodeSummariesForDay,
  eventsForDay,
  graphNodesForDay,
  priorDayRollup,
  profileFacts,
  upsertDayRollup,
  upsertDigest,
} from "../db.mjs";
import { rebuildEpisodesForDay } from "../episodes.mjs";
import { buildGraphFromEpisodes } from "../graph.mjs";
import { indexDayMemories } from "../memory/index.mjs";
import { syncProfileFromDigest } from "../memory/profile.mjs";
import { buildRagContext } from "../memory/retrieve.mjs";
import {
  buildPassAPayload,
  compactEpisode,
  compactMomentsForPass,
  compactPassJson,
} from "./compact.mjs";

export async function passA(db, day) {
  const episodes = episodeSummariesForDay(db, day);
  const moments = alignedMomentsForDay(db, day);
  const events = eventsForDay(db, day);
  const userContent = buildPassAPayload(day, episodes, moments, events);
  console.log(
    `[digest] pass A input ${userContent.length} chars · ${moments.length} moments · ${episodes.length} episodes`,
  );

  return chatJson({
    model: CFG.modelFast,
    messages: [
      {
        role: "system",
        content:
          "You are Pass A of folio digest pipeline. Extract only verifiable facts from witness data. " +
          'Reply raw JSON: {"timeline":[{"at":"ISO","fact":"","evidence":["utt:N|frm:N|ep:ID"]}],' +
          '"episode_facts":[{"episode_id":"","facts":[]}],"events_notable":[]}. ' +
          "No interpretation. No markdown. Input may be sampled — use evidence IDs as given. " +
          promptLanguageRule(),
      },
      {
        role: "user",
        content: userContent,
      },
    ],
    maxTokens: 3000,
  });
}

export async function passB(db, day, passAJson, prior, rag) {
  const episodes = episodeSummariesForDay(db, day).map(compactEpisode);
  const profile = rag?.profile?.length ? rag.profile : profileFacts(db).slice(0, 24);

  return chatJson({
    model: CFG.modelDeep,
    messages: [
      {
        role: "system",
        content:
          "You are Pass B (interpretation). Given Pass A facts and episode semantics, infer meaning: emotional arc, " +
          "decisions vs brainstorming, what was abandoned, what remains open, cross-modal alignments (speech + visual timing). " +
          "Use long_term_memory and graph_context when they relate to today — cite memory day + kind, do not invent past events. " +
          'Reply raw JSON: {"narrative_arc":"","shifts":[{"at":"","description":"","evidence":[]}],' +
          '"decisions_real":[{"text":"","evidence":[]}],"open_loops":[],"patterns":[],"tomorrow_pull":[]}. ' +
          promptLanguageRule(),
      },
      {
        role: "user",
        content: JSON.stringify(
          {
            day,
            pass_a: compactPassJson(passAJson),
            episodes,
            profile,
            prior_day: prior ? compactPassJson(prior, 1) : null,
            long_term_memory: rag?.memories ?? [],
            graph_context: rag?.graph ?? [],
          },
          null,
          2,
        ),
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
        content: JSON.stringify(
          {
            pass_a: compactPassJson(passAJson),
            pass_b: compactPassJson(passBJson),
            moments: compactMomentsForPass(moments),
          },
          null,
          2,
        ),
      },
    ],
    maxTokens: 2500,
  });
}

export async function passD(day, passAJson, passBJson, passCJson, prior, rag) {
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
          "implicit decisions, open threads for tomorrow, continuity with long_term_memory when relevant (patterns, open loops from prior days). " +
          evidenceFooterRule() +
          " Only include claims approved in Pass C.",
      },
      {
        role: "user",
        content: JSON.stringify(
          {
            title: `Folio · ${dateLabel}`,
            pass_a: compactPassJson(passAJson, 1),
            pass_b: compactPassJson(passBJson, 1),
            pass_c: compactPassJson(passCJson, 1),
            prior_day: prior ? compactPassJson(prior, 1) : null,
            long_term_memory: rag?.memories ?? [],
            graph_context: rag?.graph ?? [],
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
  const episodeSummaries = () => episodeSummariesForDay(db, day);

  console.log("[digest] pass A — facts");
  const a = await passA(db, day);

  const rag = await buildRagContext(db, day, {
    episodes: episodeSummaries(),
    passAJson: a,
  });

  console.log("[digest] pass B — interpretation");
  const b = await passB(db, day, a, prior, rag);
  const moments = alignedMomentsForDay(db, day);
  console.log("[digest] pass C — critique");
  const c = await passC(a, b, moments);
  console.log("[digest] pass D — prose");
  const prose = await passD(day, a, b, c, prior, rag);

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

  syncProfileFromDigest(db, day, b, c);

  await indexDayMemories(db, day, {
    episodes: episodes.map((ep) => ({
      id: ep.id,
      label: ep.label,
      summary: ep.summary,
    })),
    passB: b,
    passC: c,
    prose,
    graphNodes: graphNodesForDay(db, day),
  });

  return { day, prose, passes: { a, b, c }, rag };
}
