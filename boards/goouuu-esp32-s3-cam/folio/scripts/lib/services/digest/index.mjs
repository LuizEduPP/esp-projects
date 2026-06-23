import { randomUUID } from "node:crypto";
import { writeFileSync } from "node:fs";
import { join } from "node:path";
import { CFG, PATHS } from "../../config/index.mjs";
import { chatCompletion, chatJson, chatJsonLenient } from "../../llm/index.mjs";
import {
  chroniclerPassBSystem,
  chroniclerPassCSystem,
  chroniclerPassDSystem,
  dateLabelForDay,
  evidenceFooterRule,
  promptLanguageRule,
  sanitizeChronicleProse,
} from "../../locale/index.mjs";
import { modelId, ModelSlot } from "../../models/index.mjs";
import { errMsg, isoNow, today } from "../../util/index.mjs";
import { parsePassBFallback, parsePassCFallback } from "../../util/json.mjs";
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
  patchDigest,
  upsertDayRollup,
  upsertDigest,
  utterancesForDay,
  witnessStats,
} from "../../db/index.mjs";
import { buildRagContext, indexDayMemories } from "../../memory/index.mjs";
import {
  buildPassAPayload,
  compactEpisode,
  compactMomentsForPass,
  compactPassJson,
  sampleEvenly,
  truncateText,
} from "./compact.mjs";

const GAP_MS = () => CFG.episodeGapMin * 60 * 1000;

/** Live digest state for UI polling. */
export const digestRuntime = {
  busy: false,
  day: null,
  phase: null,
  error: null,
  at: null,
};

function setDigestPhase(day, phase) {
  digestRuntime.busy = true;
  digestRuntime.day = day;
  digestRuntime.phase = phase;
  digestRuntime.error = null;
  digestRuntime.at = isoNow();
}

function setDigestError(day, err) {
  digestRuntime.busy = false;
  digestRuntime.day = day;
  digestRuntime.phase = "error";
  digestRuntime.error = errMsg(err);
  digestRuntime.at = isoNow();
}

function clearDigestRuntime() {
  digestRuntime.busy = false;
  digestRuntime.phase = "done";
  digestRuntime.error = null;
  digestRuntime.at = isoNow();
}

export function digestDraftFromRow(row) {
  if (!row) {
    return null;
  }
  if (row.prose?.trim()) {
    return null;
  }
  try {
    const b = JSON.parse(row.pass_b_json || "{}");
    const arc = b?.narrative_arc?.trim();
    if (!arc) {
      return null;
    }
    return arc;
  } catch {
    return null;
  }
}

function proseFromArc(day, arc) {
  const text = String(arc ?? "").trim();
  if (!text) {
    return "";
  }
  return `Folio · ${dateLabelForDay(day)}\n\n${text}`;
}

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

function episodeSummaryFallback(episode, utterances, frames) {
  const epUtterances = utterances.filter((u) => episode.utterance_ids.includes(u.id));
  const first = epUtterances[0];
  const time = (episode.started_at ?? "").slice(11, 16) || "??:??";
  const snippet = first?.text?.trim().slice(0, 72);
  return {
    label: snippet || `Episódio ${time}`,
    decisions: [],
    rejected: [],
    open_questions: [],
    themes: [],
    energy: epUtterances.length ? "fragmented" : "calm",
    visual_context: frames[0]?.caption || frames[0]?.scene_json || "",
    implicit_shifts: [],
    notable_quotes: first
      ? [{ text: first.text.slice(0, 200), evidence: [`utt:${first.id}`] }]
      : [],
  };
}

export async function extractEpisodeSemantics(episode, utterances, frames) {
  const uttTexts = utterances
    .filter((u) => episode.utterance_ids.includes(u.id))
    .map((u) => `[${u.started_at}] utt:${u.id} ${u.text}`)
    .join("\n");

  const frameTexts = frames
    .map((f) => `[${f.captured_at}] frm:${f.id} ${f.caption || f.scene_json || "(no caption)"}`)
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

  const fallback = () => episodeSummaryFallback(episode, utterances, frames);
  const parsed = await chatJsonLenient({
    model: modelId(ModelSlot.FAST),
    messages: [
      {
        role: "user",
        content: `${prompt}\n\n--- TRANSCRIPT ---\n${uttTexts || "(silence)"}\n\n--- VISUAL ---\n${frameTexts || "(no frames)"}`,
      },
    ],
    maxTokens: 1200,
    fallback,
  });

  if (!String(parsed.label ?? "").trim()) {
    return { ...fallback(), ...parsed, label: fallback().label };
  }
  return parsed;
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
    const label = String(summary.label ?? "").trim() || episodeSummaryFallback(cluster, epUtterances, epFrames).label;

    insertEpisode(db, {
      id: cluster.id,
      day,
      started_at: cluster.started_at,
      ended_at: cluster.ended_at,
      label,
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
    const text = String(label ?? "").trim();
    if (!text) {
      return null;
    }
    const key = `${kind}:${text}`;
    if (nodes.has(key)) {
      return nodes.get(key);
    }
    const id = graphNodeId(day, kind, text);
    insertGraphNode(db, {
      id,
      day,
      kind,
      label: text,
      payload_json: JSON.stringify(payload),
    });
    nodes.set(key, id);
    return id;
  };

  const link = (from, to, relation, evidence, confidence) => {
    if (!from || !to) {
      return;
    }
    insertGraphEdge(db, {
      day,
      from_node: from,
      to_node: to,
      relation,
      evidence_json: JSON.stringify(evidence),
      confidence,
    });
  };

  for (const ep of episodes) {
    const summary = ep.summary ?? JSON.parse(ep.summary_json || "{}");
    const epNode = ensureNode("episode", summary.label || ep.label || ep.id, {
      started_at: ep.started_at,
      ended_at: ep.ended_at,
    });

    for (const theme of summary.themes ?? []) {
      link(epNode, ensureNode("theme", theme), "themed", [`ep:${ep.id}`], CFG.episodeGraphThemedConfidence);
    }

    for (const d of summary.decisions ?? []) {
      link(
        epNode,
        ensureNode("decision", d.text, { confidence: d.confidence }),
        "decided",
        d.evidence ?? [],
        d.confidence ?? CFG.episodeGraphDecidedConfidence,
      );
    }

    for (const q of summary.open_questions ?? []) {
      link(epNode, ensureNode("question", q), "left_open", [`ep:${ep.id}`], CFG.episodeGraphOpenConfidence);
    }

    for (const r of summary.rejected ?? []) {
      link(epNode, ensureNode("rejected", r), "rejected", [`ep:${ep.id}`], CFG.episodeGraphRejectedConfidence);
    }
  }

  return { nodeCount: nodes.size };
}

function buildCrossModalSample(moments, windowMs = CFG.episodeFrameAlignMs, maxPairs = 28) {
  const utterances = moments.filter((m) => m.text);
  const frames = moments.filter((m) => m.visual);
  const pairs = [];

  for (const u of utterances) {
    const t = Date.parse(u.at);
    let best = null;
    let bestDelta = Infinity;
    for (const f of frames) {
      const delta = Math.abs(Date.parse(f.at) - t);
      if (delta <= windowMs && delta < bestDelta) {
        best = f;
        bestDelta = delta;
      }
    }
    if (best) {
      pairs.push({
        at: u.at,
        speech: truncateText(u.text, 200),
        visual: truncateText(best.visual, 140),
        speech_id: u.id,
        visual_id: best.id,
        delta_ms: bestDelta,
      });
    }
  }

  return sampleEvenly(pairs, maxPairs);
}

function episodeTimeline(episodes) {
  return episodes.map((ep) => ({
    id: ep.id,
    label: truncateText(ep.label, 80),
    started_at: ep.started_at,
    ended_at: ep.ended_at,
    energy: ep.summary?.energy,
    themes: (ep.summary?.themes ?? []).slice(0, 4),
    rejected: (ep.summary?.rejected ?? []).slice(0, 4),
  }));
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

export async function passB(db, day, passAJson, prior, rag, moments) {
  const episodes = episodeSummariesForDay(db, day).map(compactEpisode);
  const profile = rag?.profile?.length ? rag.profile : profileFacts(db).slice(0, CFG.memoryProfileLimit);
  const aligned = compactMomentsForPass(moments);
  const crossModal = buildCrossModalSample(moments);

  return chatJsonLenient({
    model: modelId(ModelSlot.DEEP),
    temperature: 0.05,
    messages: [
      {
        role: "system",
        content: chroniclerPassBSystem(),
      },
      {
        role: "user",
        content: JSON.stringify(
          {
            day,
            pass_a: compactPassJson(passAJson),
            episodes,
            episode_timeline: episodeTimeline(episodes),
            aligned_moments: aligned.slice(0, 24),
            cross_modal_candidates: crossModal.slice(0, 12),
            profile,
            prior_day: prior ? compactPassJson(prior, 1) : null,
            long_term_memory: (rag?.memories ?? []).slice(0, 8),
            graph_context: (rag?.graph ?? []).slice(0, 6),
          },
          null,
          2,
        ),
      },
    ],
    maxTokens: 2200,
    fallback: (text) => parsePassBFallback(text),
  });
}

export async function passC(passAJson, passBJson, moments) {
  return chatJsonLenient({
    model: modelId(ModelSlot.FAST),
    messages: [
      {
        role: "system",
        content: chroniclerPassCSystem(),
      },
      {
        role: "user",
        content: JSON.stringify(
          {
            pass_a: compactPassJson(passAJson),
            pass_b: compactPassJson(passBJson),
            moments: compactMomentsForPass(moments).slice(0, 20),
          },
          null,
          2,
        ),
      },
    ],
    maxTokens: 1800,
    fallback: (text) => parsePassCFallback(text, passBJson),
  });
}

export async function passD(day, passAJson, passBJson, passCJson, prior, rag, { existingProse = null } = {}) {
  const dateLabel = dateLabelForDay(day);
  const incremental = Boolean(existingProse?.trim());

  const raw = await chatCompletion({
    model: modelId(ModelSlot.DEEP),
    temperature: CFG.digestPassDTemperature,
    maxTokens: 2800,
    messages: [
      {
        role: "system",
        content:
          chroniclerPassDSystem({ incremental }) +
          " " +
          evidenceFooterRule() +
          " Only include claims approved in Pass C; cross-modal scenes only from approved_cross_modal.",
      },
      {
        role: "user",
        content: JSON.stringify(
          {
            title: `Folio · ${dateLabel}`,
            existing_chronicle: incremental ? existingProse : null,
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

  const cleaned = sanitizeChronicleProse(raw);
  if (cleaned.length >= 80) {
    return cleaned;
  }
  return sanitizeChronicleProse(proseFromArc(day, passBJson?.narrative_arc));
}

export async function runDigestPipeline(db, day, { existingProse = null } = {}) {
  setDigestPhase(day, "episodes");
  console.log(`[digest] rebuilding episodes for ${day}`);
  const episodes = await rebuildEpisodesForDay(db, day);
  buildGraphFromEpisodes(db, day, episodes);

  const prior = priorDayRollup(db, day);
  const episodeSummaries = () => episodeSummariesForDay(db, day);
  const moments = alignedMomentsForDay(db, day);

  setDigestPhase(day, "pass_a");
  console.log("[digest] pass A — facts");
  const a = await passA(db, day);
  patchDigest(db, day, { pass_a_json: JSON.stringify(a) });

  const rag = await buildRagContext(db, day, {
    episodes: episodeSummaries(),
    passAJson: a,
  });

  setDigestPhase(day, "pass_b");
  console.log("[digest] pass B — interpretation");
  const b = await passB(db, day, a, prior, rag, moments);
  patchDigest(db, day, { pass_b_json: JSON.stringify(b) });

  setDigestPhase(day, "pass_c");
  console.log("[digest] pass C — critique");
  const c = await passC(a, b, moments);
  patchDigest(db, day, { pass_c_json: JSON.stringify(c) });

  setDigestPhase(day, "pass_d");
  console.log(`[digest] pass D — prose${existingProse ? " (incremental)" : ""}`);
  const prose = await passD(day, a, b, c, prior, rag, { existingProse });

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

  const existing = getDigest(db, day);
  try {
    const result = await runDigestPipeline(db, day, {
      existingProse: !force && existing?.prose ? existing.prose : null,
    });
    const mdPath = saveDigestMarkdown(day, result.prose);
    clearDigestRuntime();
    console.log(`[digest] ${day} saved ${mdPath} (${check.reason ?? "forced"})`);
    return { skipped: false, day, prose: result.prose, path: mdPath };
  } catch (err) {
    setDigestError(day, err);
    const row = getDigest(db, day);
    const draft = digestDraftFromRow(row);
    if (draft) {
      const fallbackProse = proseFromArc(day, draft);
      patchDigest(db, day, { prose: fallbackProse });
      saveDigestMarkdown(day, fallbackProse);
      console.warn(`[digest] ${day} saved draft from pass B after error: ${errMsg(err)}`);
      return { skipped: false, day, prose: fallbackProse, draft: true, error: errMsg(err) };
    }
    throw err;
  }
}

export function startDigestLoop(intervalMs = CFG.digestIntervalMs) {
  let loopBusy = false;
  let lastDay = today();
  const checkMs = Math.min(intervalMs, 300_000);

  console.log(
    `[digest] auto every ${checkMs}ms — refreshes when new witness data arrives`,
  );

  const tick = async () => {
    if (loopBusy || digestRuntime.busy) {
      return;
    }
    loopBusy = true;
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
      loopBusy = false;
    }
  };

  setInterval(tick, checkMs);
  setTimeout(tick, 15_000);
}
