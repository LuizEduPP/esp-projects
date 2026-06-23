import { formatSceneCaption, sceneFingerprint } from "../llm/scene-caption.mjs";

const MS = (n) => n;

function gapMs(a, b) {
  return Math.abs(new Date(a).getTime() - new Date(b).getTime());
}

function captionKey(caption) {
  return String(caption ?? "")
    .toLowerCase()
    .replace(/\s+/g, " ")
    .trim()
    .slice(0, 96);
}

function hourLabel(iso) {
  try {
    return new Date(iso).toLocaleTimeString("pt-BR", { hour: "2-digit", minute: "2-digit" });
  } catch {
    return iso?.slice(11, 16) ?? "";
  }
}

/** Turn flat witness items into human-readable groups (conversations, scenes). */
export function groupTimelineItems(items, opts = {}) {
  const speechGapMs = opts.speechGapMs ?? MS(45_000);
  const sceneGapMs = opts.sceneGapMs ?? MS(8 * 60_000);
  const sorted = [...items].sort((a, b) => a.at.localeCompare(b.at));
  const groups = [];

  for (const item of sorted) {
    if (item.type === "audio" && item.text) {
      const last = groups.at(-1);
      const sameSpeaker =
        !item.speaker_id || !last?.speaker_id || last.speaker_id === item.speaker_id;
      if (
        last?.type === "speech" &&
        sameSpeaker &&
        gapMs(last.at_end, item.at) < speechGapMs
      ) {
        last.lines.push({
          at: item.at,
          text: item.text,
          chunk_id: item.chunk_id,
          has_pcm: item.has_pcm,
        });
        last.at_end = item.at;
        continue;
      }
      groups.push({
        type: "speech",
        at: item.at,
        at_end: item.at,
        speaker_id: item.speaker_id ?? null,
        lines: [
          {
            at: item.at,
            text: item.text,
            chunk_id: item.chunk_id,
            has_pcm: item.has_pcm,
          },
        ],
      });
      continue;
    }

    if (item.type === "frame") {
      const key = captionKey(item.caption);
      const last = groups.at(-1);
      if (
        last?.type === "scene" &&
        key &&
        last.caption_key === key &&
        gapMs(last.at_end, item.at) < sceneGapMs
      ) {
        last.frame_ids.push(item.frame_id);
        last.at_end = item.at;
        last.count += 1;
        continue;
      }
      groups.push({
        type: item.caption ? "scene" : "frame_pending",
        at: item.at,
        at_end: item.at,
        caption: item.caption ?? null,
        caption_key: key,
        frame_ids: [item.frame_id],
        count: 1,
        processed: !!item.processed,
      });
    }
  }

  return groups.reverse();
}

export function timelineWithGroups(items, opts) {
  const groups = groupTimelineItems(items, opts);
  let lastHour = null;
  const enriched = groups.map((g) => {
    const hour = g.at?.slice(11, 13) ?? "?";
    const showHour = hour !== lastHour;
    lastHour = hour;
    const label =
      g.type === "speech"
        ? g.lines.length > 1
          ? "Conversa"
          : "Fala"
        : g.type === "scene"
          ? g.count > 1
            ? "Cena contínua"
            : "Cena"
          : "Aguardando";
    return { ...g, hour, showHour, hour_label: hourLabel(g.at), kind_label: label };
  });
  return { groups: enriched, count: enriched.length };
}

export { formatSceneCaption, sceneFingerprint };
