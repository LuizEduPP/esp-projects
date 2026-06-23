/** Shrink witness payloads to fit LM context. */

export function truncateText(text, max = 240) {
  const s = String(text ?? "").trim();
  if (s.length <= max) {
    return s;
  }
  return `${s.slice(0, max - 1)}…`;
}

export function sampleEvenly(items, max) {
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

export function compactMoment(m) {
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

export function compactEvents(events, max = 36) {
  const notable = events.filter((e) => e.kind === "presence" || e.kind === "frame");
  return sampleEvenly(notable, max).map((e) => ({
    id: e.id,
    at: e.at,
    kind: e.kind,
  }));
}

export function compactPassJson(obj, maxDepth = 2) {
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

export function compactMomentsForPass(moments, maxChars = 18_000) {
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

export function buildPassAPayload(day, episodes, moments, events, maxChars = 28_000) {
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
  }

  return JSON.stringify(payload, null, 2);
}
