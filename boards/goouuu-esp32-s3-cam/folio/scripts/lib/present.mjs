import { formatSceneCaption, sceneFingerprint } from "./llm/scene.mjs";

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

/** Turn flat witness items into human-readable groups (conversations, scenes, sounds). */
export function groupTimelineItems(items, opts = {}) {
  const speechGapMs = opts.speechGapMs ?? MS(45_000);
  const sceneGapMs = opts.sceneGapMs ?? MS(8 * 60_000);
  const soundGapMs = opts.soundGapMs ?? MS(20_000);
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

    if (
      item.type === "audio" &&
      item.sound_kind &&
      item.sound_kind !== "speech" &&
      !item.text
    ) {
      const last = groups.at(-1);
      if (
        last?.type === "sound" &&
        last.sound_kind === item.sound_kind &&
        gapMs(last.at_end, item.at) < soundGapMs
      ) {
        last.count += 1;
        last.at_end = item.at;
        last.chunk_ids.push(item.chunk_id);
        continue;
      }
      groups.push({
        type: "sound",
        at: item.at,
        at_end: item.at,
        sound_kind: item.sound_kind,
        sound_label: item.sound_label || item.sound_kind,
        chunk_ids: [item.chunk_id],
        count: 1,
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
        reason: item.reason ?? null,
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
        : g.type === "sound"
          ? g.count > 1
            ? `${g.sound_label}s`
            : g.sound_label || "Som"
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
