import { CFG } from "./config.mjs";
import { activeLocale } from "./locale.mjs";
import { formatSceneCaption, sceneFingerprint } from "./llm.mjs";
import { isDarkSceneCaption, isSttHallucination } from "./util.mjs";

function gapMs(a, b) {
  return Math.abs(new Date(a).getTime() - new Date(b).getTime());
}

function parseSceneJson(raw) {
  if (!raw) {
    return null;
  }
  try {
    return JSON.parse(raw);
  } catch {
    return null;
  }
}

function normalizeCaption(caption) {
  return String(caption ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/^parece que\s+/i, "")
    .replace(/^uma pessoa\s+/i, "pessoa ")
    .replace(/[^\p{L}\p{N}\s]/gu, " ")
    .replace(/\s+/g, " ")
    .trim()
    .slice(0, 96);
}

function sceneGroupKey(item) {
  const scene = parseSceneJson(item.scene_json);
  if (scene && !scene.skipped && !scene.unchanged) {
    const fp = sceneFingerprint(scene);
    if (fp && fp !== "unchanged") {
      return fp;
    }
  }
  return normalizeCaption(item.caption);
}

export function isStaticFrameItem(item) {
  if (item.type !== "frame") {
    return false;
  }
  if (!item.caption) {
    return true;
  }
  if (item.caption === CFG.frameStaticSummary) {
    return true;
  }
  const scene = parseSceneJson(item.scene_json);
  if (scene?.skipped || scene?.unchanged || scene?.quiet || scene?.hallucination_guard) {
    return true;
  }
  if (scene?.activity === "quiet" && !scene?.person_present) {
    return true;
  }
  if (
    /smartphone|tablet|celular|cama|deitad|dispositivo pequeno|assistindo algo em um dispositivo/i.test(
      item.caption,
    ) &&
    (scene?.lighting === "dark" ||
      scene?.lighting === "very_dark" ||
      (scene?.brightness != null && scene.brightness < 0.22))
  ) {
    return true;
  }
  if (isDarkSceneCaption(item.caption)) {
    return true;
  }
  return false;
}

export function isMeaningfulFrameItem(item) {
  return item.type === "frame" && !!item.caption && !isStaticFrameItem(item);
}

/** Drop static/processed-noise frames before grouping — life-log style feed. */
export function filterWitnessItems(items, opts = {}) {
  const hideStatic = opts.hideStatic ?? CFG.presentHideStaticFrames ?? true;
  const hidePending = opts.hidePending ?? CFG.presentHidePendingInFeed ?? true;

  return items.filter((item) => {
    if (item.type === "frame") {
      if (hideStatic && isStaticFrameItem(item)) {
        return false;
      }
      if (hidePending && !item.processed) {
        return false;
      }
    }
    return true;
  });
}

function hourLabel(iso) {
  try {
    return new Date(iso).toLocaleTimeString(activeLocale(), { hour: "2-digit", minute: "2-digit" });
  } catch {
    return iso?.slice(11, 16) ?? "";
  }
}

function presentLabels() {
  return CFG.presentLabels ?? {};
}

/** Turn flat witness items into human-readable groups (conversations, scenes, sounds). */
export function groupTimelineItems(items, opts = {}) {
  const speechGapMs = opts.speechGapMs ?? CFG.presentSpeechGapMs;
  const sceneGapMs = opts.sceneGapMs ?? CFG.presentSceneGapMs;
  const soundGapMs = opts.soundGapMs ?? CFG.presentSoundGapMs;
  const sorted = [...items].sort((a, b) => a.at.localeCompare(b.at));
  const groups = [];

  for (const item of sorted) {
    if (item.type === "audio" && item.text) {
      const text = String(item.text).trim();
      if (!text || isSttHallucination(text, CFG.audioSttRejectPatterns)) {
        continue;
      }
      const last = groups.at(-1);
      const sameSpeaker =
        !item.speaker_id || !last?.speaker_id || last.speaker_id === item.speaker_id;
      const dupLine =
        last?.type === "speech" &&
        last.lines.some((ln) => ln.text.trim().toLowerCase() === text.toLowerCase());
      if (dupLine) {
        continue;
      }
      if (
        last?.type === "speech" &&
        sameSpeaker &&
        gapMs(last.at_end, item.at) < speechGapMs
      ) {
        last.lines.push({
          at: item.at,
          text,
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
            text,
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
      if (!item.caption) {
        const last = groups.at(-1);
        if (last?.type === "frame_pending") {
          last.frame_ids.push(item.frame_id);
          last.at_end = item.at;
          last.count += 1;
          continue;
        }
        groups.push({
          type: "frame_pending",
          at: item.at,
          at_end: item.at,
          caption: null,
          caption_key: "",
          frame_ids: [item.frame_id],
          reason: item.reason ?? null,
          count: 1,
          processed: false,
        });
        continue;
      }

      const key = sceneGroupKey(item);
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
        type: "scene",
        at: item.at,
        at_end: item.at,
        caption: item.caption ?? null,
        caption_key: key,
        frame_ids: [item.frame_id],
        reason: item.reason ?? null,
        scene_json: item.scene_json ?? null,
        count: 1,
        processed: !!item.processed,
      });
    }
  }

  return groups.reverse();
}

export function timelineWithGroups(items, opts) {
  const filtered = filterWitnessItems(items, opts);
  const groups = groupTimelineItems(filtered, opts);
  const labels = presentLabels();
  let lastHour = null;
  const enriched = groups.map((g) => {
    const hour = g.at?.slice(11, 13) ?? "?";
    const showHour = hour !== lastHour;
    lastHour = hour;
    const label =
      g.type === "speech"
        ? g.lines.length > 1
          ? labels.speechGroup
          : labels.speechSingle
        : g.type === "sound"
          ? g.count > 1
            ? `${g.sound_label}${labels.soundPluralSuffix ?? ""}`
            : g.sound_label || labels.soundFallback
        : g.type === "scene"
          ? g.count > 1
            ? labels.sceneGroup
            : labels.sceneSingle
          : g.type === "frame_pending"
            ? g.count > 1
              ? `${g.count} ${labels.pendingPlural ?? labels.pending}`
              : labels.pending
            : labels.pending;
    return { ...g, hour, showHour, hour_label: hourLabel(g.at), kind_label: label };
  });
  return {
    groups: enriched,
    count: enriched.length,
    scenes: enriched.filter((g) => g.type === "scene").length,
    filtered_from: items.length - filtered.length,
  };
}

export { formatSceneCaption, sceneFingerprint };
