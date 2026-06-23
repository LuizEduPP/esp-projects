import { randomUUID } from "node:crypto";
import { CFG } from "./config.mjs";
import { chatJson } from "./lm.mjs";
import { promptLanguageRule } from "./locale.mjs";
import { isoNow } from "./util.mjs";
import {
  clearEpisodesForDay,
  framesForDay,
  insertEpisode,
  linkEpisodeFrame,
  linkEpisodeUtterance,
  utterancesForDay,
} from "./db.mjs";

const GAP_MS = () => CFG.episodeGapMin * 60 * 1000;

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

  const parsed = await chatJson({
    model: CFG.modelFast,
    messages: [
      {
        role: "user",
        content: `${prompt}\n\n--- TRANSCRIPT ---\n${uttTexts || "(silence)"}\n\n--- VISUAL ---\n${frameTexts || "(no frames)"}`,
      },
    ],
    maxTokens: 1200,
  });

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
