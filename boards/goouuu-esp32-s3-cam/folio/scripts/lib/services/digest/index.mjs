import { randomUUID } from "node:crypto";
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "../../config/index.mjs";
import { chatCompletion, chatJson } from "../../llm/index.mjs";
import { dateLabelForDay, evidenceFooterRule, promptLanguageRule } from "../../locale/index.mjs";
import { modelId, ModelSlot } from "../../models/index.mjs";
import { errMsg, isoNow, today } from "../../util/index.mjs";
import {
  alignedMomentsForDay,
  clearEpisodesForDay,
  episodeSummariesForDay,
  eventsForDay,
  framesForDay,
  getDigest,
  graphNodesForDay,
  insertEpisode,
  insertGraphEdge,
  insertGraphNode,
  latestWitnessAt,
  linkEpisodeFrame,
  linkEpisodeUtterance,
  openDb,
  priorDayRollup,
  profileFacts,
  upsertDayRollup,
  upsertDigest,
  utterancesForDay,
  witnessStats,
} from "../../db/index.mjs";
import { buildRagContext, indexDayMemories } from "../../memory/index.mjs";

const GAP_MS = () => CFG.episodeGapMin * 60 * 1000;

function graphNodeId(day, kind, label) {
  const slug = label
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .slice(0, 40);
  return `gn-${day}-${kind}-${slug}-${randomUUID().slice(0, 6)}`;
}

function alignFrameToEpisode(frameAt, episodes) {
  const windowMs = CFG.episodeFrameAlignMs;
  const t = Date.parse(frameAt);
  for (const ep of episodes) {
    const start = Date.parse(ep.started_at);
    const end = Date.parse(ep.ended_at);
    if (t >= start - windowMs && t <= end + windowMs) {
      return ep.id;
    }
  }
  return null;
}

export function clusterUtterances(utterances, day) {
  if (!utterances.length) {
    return [];
  }
  const gap = GAP_MS();
  const episodes = [];
  let current = {
    id: `ep-${day}-${randomUUID().slice(0, 8)}`,
    day,
    started_at: utterances[0].started_at,
    ended_at: utterances[0].ended_at,
    utterance_ids: [utterances[0].id],
  };

  for (let i = 1; i < utterances.length; i++) {
    const u = utterances[i];
    const prev = utterances[i - 1];
    const delta = Date.parse(u.started_at) - Date.parse(prev.ended_at);
    if (delta > gap) {
      episodes.push(current);
      current = {
        id: `ep-${day}-${randomUUID().slice(0, 8)}`,
        day,
        started_at: u.started_at,
        ended_at: u.ended_at,
        utterance_ids: [u.id],
      };
    } else {
      current.ended_at = u.ended_at;
      current.utterance_ids.push(u.id);
    }
  }
  episodes.push(current);
  return episodes;
}

export async function extractEpisodeSemantics(episode, utterances, frames) {
  const uttTexts = utterances
    .filter((u) => episode.utterance_ids.includes(u.id))
    .map((u) => `[${u.started_at}] ${u.text}`)
    .join("\n");

  const frameTexts = frames
    .map((f) => `[${f.captured_at}] ${f.caption || f.scene_json || "(no caption)"}`)
    .join("\n");

  const prompt = `You analyze a slice of someone's day from passive room witness data (audio transcript + camera captions).
Reply raw JSON only:
{
  "label": "short episode title",
  "decisions": [{"text":"","confidence":0-1,"evidence":["utt:ID or frm:ID"]}],
  "rejected": ["ideas explicitly abandoned"],
  "open_questions": ["unresolved"],
  "themes": ["up to 5"],
  "energy": "fragmented|focused|frustrated|calm|social|high_clarity",
  "visual_context": "one line",
  "implicit_shifts": ["what changed beneath the words"],
  "notable_quotes": [{"text":"","evidence":["utt:ID"]}]
}
Use evidence IDs from the input when possible. Facts only from input; infer carefully. ${promptLanguageRule()}`;

  return chatJson({
    model: modelId(ModelSlot.FAST),
    messages: [
      {
        role: "user",
        content: `${prompt}\n\n--- TRANSCRIPT ---\n${uttTexts || "(silence)"}\n\n--- VISUAL ---\n${frameTexts || "(no frames)"}`,
      },
    ],
    maxTokens: 1200,
  });
}

export async function rebuildEpisodesForDay(db, day) {
  const utterances = utterancesForDay(db, day);
  const frames = framesForDay(db, day);

  clearEpisodesForDay(db, day);
  const clusters = clusterUtterances(utterances, day);
  const built = [];

  for (const cluster of clusters) {
    const epUtterances = utterances.filter((u) => cluster.utterance_ids.includes(u.id));
    const epFrames = frames.filter((f) => alignFrameToEpisode(f.captured_at, [cluster]));
    const summary = await extractEpisodeSemantics(cluster, epUtterances, epFrames);

    insertEpisode(db, {
      id: cluster.id,
      day,
      started_at: cluster.started_at,
      ended_at: cluster.ended_at,
      label: summary.label,
      summary_json: JSON.stringify(summary),
      created_at: isoNow(),
    });

    for (const uid of cluster.utterance_ids) {
      linkEpisodeUtterance(db, cluster.id, uid);
    }
    for (const f of epFrames) {
      linkEpisodeFrame(db, cluster.id, f.id);
    }

    built.push({ ...cluster, summary });
  }

  return built;
}

export function buildGraphFromEpisodes(db, day, episodes) {
  const nodes = new Map();

  const ensureNode = (kind, label, payload = {}) => {
    const key = `${kind}:${label}`;
    if (nodes.has(key)) {
      return nodes.get(key);
    }
    const id = graphNodeId(day, kind, label);
    insertGraphNode(db, {
      id,
      day,
      kind,
      label,
      payload_json: JSON.stringify(payload),
    });
    nodes.set(key, id);
    return id;
  };

  for (const ep of episodes) {
    const summary = ep.summary ?? JSON.parse(ep.summary_json || "{}");
    const epNode = ensureNode("episode", summary.label || ep.label || ep.id, {
      started_at: ep.started_at,
      ended_at: ep.ended_at,
    });

    for (const theme of summary.themes ?? []) {
      const themeNode = ensureNode("theme", theme);
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: themeNode,
        relation: "themed",
        evidence_json: JSON.stringify([`ep:${ep.id}`]),
        confidence: CFG.episodeGraphThemedConfidence,
      });
    }

    for (const d of summary.decisions ?? []) {
      const decNode = ensureNode("decision", d.text, { confidence: d.confidence });
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: decNode,
        relation: "decided",
        evidence_json: JSON.stringify(d.evidence ?? []),
        confidence: d.confidence ?? CFG.episodeGraphDecidedConfidence,
      });
    }

    for (const q of summary.open_questions ?? []) {
      const qNode = ensureNode("question", q);
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: qNode,
        relation: "left_open",
        evidence_json: JSON.stringify([`ep:${ep.id}`]),
        confidence: CFG.episodeGraphOpenConfidence,
      });
    }

    for (const r of summary.rejected ?? []) {
      const rNode = ensureNode("rejected", r);
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: rNode,
        relation: "rejected",
        evidence_json: JSON.stringify([`ep:${ep.id}`]),
        confidence: CFG.episodeGraphRejectedConfidence,
      });
    }
  }

  return { nodeCount: nodes.size };
}

const COMPACT_MAX_CHARS = 28_000;

function truncateText(text, max = 240) {
  const s = String(text ?? "").trim();
  if (s.length <= max) {
    return s;
  }
  return `${s.slice(0, max - 1)}…`;
}

function sampleEvenly(items, max) {
  if (!items?.length || items.length <= max) {
    return items ?? [];
  }
  const out = [];
  const step = items.length / max;
  for (let i = 0; i < max; i++) {
    out.push(items[Math.floor(i * step)]);
  }
  return out;
}

function compactMoment(m) {
  if (m.text) {
    return { id: m.id, at: m.at, text: truncateText(m.text, 320) };
  }
  return { id: m.id, at: m.at, visual: truncateText(m.visual, 180) };
}

export function compactEpisode(ep) {
  const s = ep.summary ?? {};
  return {
    id: ep.id,
    label: truncateText(ep.label, 80),
    started_at: ep.started_at,
    ended_at: ep.ended_at,
    summary: {
      label: truncateText(s.label, 80),
      themes: (s.themes ?? []).slice(0, 5).map((t) => truncateText(t, 60)),
      decisions: (s.decisions ?? []).slice(0, 6).map((d) => ({
        text: truncateText(d.text, 120),
        evidence: (d.evidence ?? []).slice(0, 4),
      })),
      open_questions: (s.open_questions ?? []).slice(0, 5).map((q) => truncateText(q, 100)),
      energy: s.energy,
      visual_context: truncateText(s.visual_context, 120),
      notable_quotes: (s.notable_quotes ?? []).slice(0, 4).map((q) => ({
        text: truncateText(q.text, 120),
        evidence: (q.evidence ?? []).slice(0, 2),
      })),
    },
  };
}

function compactEvents(events, max = 36) {
  const notable = events.filter((e) => e.kind === "presence" || e.kind === "frame");
  return sampleEvenly(notable, max).map((e) => ({
    id: e.id,
    at: e.at,
    kind: e.kind,
  }));
}

function compactPassJson(obj, maxDepth = 2) {
  if (maxDepth <= 0 || obj == null) {
    return obj;
  }
  if (Array.isArray(obj)) {
    return obj.slice(0, 40).map((v) =>
      typeof v === "object" ? compactPassJson(v, maxDepth - 1) : truncateText(v, 200),
    );
  }
  if (typeof obj === "object") {
    const out = {};
    for (const [k, v] of Object.entries(obj).slice(0, 30)) {
      if (typeof v === "string") {
        out[k] = truncateText(v, 400);
      } else if (typeof v === "object") {
        out[k] = compactPassJson(v, maxDepth - 1);
      } else {
        out[k] = v;
      }
    }
    return out;
  }
  return truncateText(obj, 200);
}

function buildPassAPayload(day, episodes, moments, events, maxChars = COMPACT_MAX_CHARS) {
  let momentLimit = Math.min(moments.length, 80);
  let payload = {
    day,
    episodes: episodes.map(compactEpisode),
    moments: sampleEvenly(moments, momentLimit).map(compactMoment),
    events: compactEvents(events),
  };

  let text = JSON.stringify(payload, null, 2);
  while (text.length > maxChars && momentLimit > 12) {
    momentLimit = Math.max(12, Math.floor(momentLimit * 0.65));
    payload.moments = sampleEvenly(moments, momentLimit).map(compactMoment);
    text = JSON.stringify(payload, null, 2);
  }

  if (text.length > maxChars) {
    payload.moments = payload.moments.map((m) => ({
      ...m,
      text: m.text ? truncateText(m.text, 120) : undefined,
      visual: m.visual ? truncateText(m.visual, 80) : undefined,
    }));
    text = JSON.stringify(payload, null, 2);
  }

  if (text.length > maxChars) {
    console.warn(`[digest] pass A input still ${text.length} chars after compaction`);
  }

  return text;
}

function compactMomentsForPass(moments, maxChars = 18_000) {
  let limit = Math.min(moments.length, 60);
  let compact = sampleEvenly(moments, limit).map(compactMoment);
  let text = JSON.stringify(compact);
  while (text.length > maxChars && limit > 8) {
    limit = Math.max(8, Math.floor(limit * 0.65));
    compact = sampleEvenly(moments, limit).map(compactMoment);
    text = JSON.stringify(compact);
  }
  return compact;
}

export async function passA(db, day) {
  const episodes = episodeSummariesForDay(db, day);
  const moments = alignedMomentsForDay(db, day);
  const events = eventsForDay(db, day);
  const userContent = buildPassAPayload(day, episodes, moments, events);
  console.log(
    `[digest] pass A input ${userContent.length} chars · ${moments.length} moments · ${episodes.length} episodes`,
  );

  return chatJson({
    model: modelId(ModelSlot.FAST),
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
  const profile = rag?.profile?.length ? rag.profile : profileFacts(db).slice(0, CFG.memoryProfileLimit);

  return chatJson({
    model: modelId(ModelSlot.DEEP),
    messages: [
      {
        role: "system",
        content:
          "You are Pass B (interpretation). Given Pass A facts and episode semantics, infer meaning: emotional arc, " +
          "decisions vs brainstorming, what was abandoned, what remains open, cross-modal alignments (speech + visual timing). " +
          "Use long_term_memory and graph_context when they relate to today — cite memory day + kind, do not invent past events. " +
          'Reply with valid JSON only — no markdown fences, no // comments, no line breaks inside strings, no **bold**: ' +
          '{"narrative_arc":"","shifts":[{"at":"","description":"","evidence":[]}],' +
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
    model: modelId(ModelSlot.FAST),
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
    model: modelId(ModelSlot.DEEP),
    temperature: CFG.digestPassDTemperature,
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
    model_fast: modelId(ModelSlot.FAST),
    model_deep: modelId(ModelSlot.DEEP),
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


export function saveDigestMarkdown(day, prose) {
  const mdPath = join(PATHS.digestDir(), `${day}.md`);
  writeFileSync(mdPath, prose ?? "", "utf8");
  return mdPath;
}

export function needsDigestRefresh(db, day) {
  const stats = witnessStats(db, day);
  if (stats.frames === 0 && stats.utterances === 0 && stats.speech === 0) {
    return { run: false, reason: "no_witness_data" };
  }

  const latest = latestWitnessAt(db, day);
  if (!latest) {
    return { run: false, reason: "no_timestamps" };
  }

  const existing = getDigest(db, day);
  if (!existing?.prose) {
    return { run: true, reason: "first_digest", stats, latest };
  }

  if (latest > existing.updated_at) {
    return { run: true, reason: "new_witness_data", stats, latest };
  }

  return { run: false, reason: "up_to_date", stats, latest };
}

export async function runDigestForDay(db, day, { force = false } = {}) {
  const check = needsDigestRefresh(db, day);
  if (!force && !check.run) {
    return { skipped: true, day, reason: check.reason };
  }

  const result = await runDigestPipeline(db, day);
  const mdPath = saveDigestMarkdown(day, result.prose);
  console.log(`[digest] ${day} saved ${mdPath} (${check.reason ?? "forced"})`);
  return { skipped: false, day, prose: result.prose, path: mdPath };
}

export function startDigestLoop(intervalMs = CFG.digestIntervalMs) {
  let busy = false;
  let lastDay = today();

  console.log(`[digest] auto every ${intervalMs}ms — refreshes when new witness data arrives`);

  const tick = async () => {
    if (busy) {
      return;
    }
    busy = true;
    try {
      const db = openDb();
      const currentDay = today();

      if (currentDay !== lastDay) {
        const prior = lastDay;
        lastDay = currentDay;
        console.log(`[digest] day rollover — finalizing ${prior}`);
        await runDigestForDay(db, prior, { force: true });
      }

      await runDigestForDay(db, currentDay);
    } catch (err) {
      console.error(`[digest] ${errMsg(err)}`);
    } finally {
      busy = false;
    }
  };

  setInterval(tick, intervalMs);
  setTimeout(tick, 20000);
}
