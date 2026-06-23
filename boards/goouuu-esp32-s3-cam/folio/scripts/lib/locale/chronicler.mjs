import { promptLanguageRule } from "./index.mjs";

const FORBIDDEN_OUTPUT =
  "FORBIDDEN in final prose: markdown headings (##), bullet lists (- or *), numbered lists, " +
  "section labels (Resumo, Summary, Insights, O que foi visto, Assuntos conversados, " +
  "What was seen, Topics discussed), or report/checklist tone.";

export function chroniclerPassBSystem() {
  return (
    "You are Pass B (interpretation) for folio — a passive day witness on the user's desk. " +
    "You read aligned speech + camera moments and episode semantics. Your job is insight the user " +
    "could not get by re-reading transcripts: emotional arc, real decisions vs brainstorming, " +
    "ideas explicitly abandoned, cross-modal alignments (what was said while the camera showed X), " +
    "implicit shifts, contradictions, energy changes, and continuity with long_term_memory. " +
    "Cite evidence IDs (utt:N, frm:N, ep:ID). Do not invent past events beyond long_term_memory. " +
    'Reply valid JSON only — no markdown fences, no // comments, no **bold**, no real line breaks inside strings: ' +
    '{"narrative_arc":"one paragraph max 500 chars","day_phases":[{"period":"morning|afternoon|evening|night","focus":"","energy":""}],' +
    '"shifts":[{"at":"ISO","description":"","evidence":[]}],' +
    '"cross_modal":[{"at":"ISO","speech":"","visual":"","inference":"","evidence":[]}],' +
    '"decisions_real":[{"text":"","evidence":[],"confidence":0-1}],' +
    '"rejected":[{"text":"","evidence":[]}],' +
    '"implicit_threads":[{"text":"","evidence":[]}],"open_loops":[],"patterns":[],"tomorrow_pull":[]}. ' +
    promptLanguageRule()
  );
}

export function chroniclerPassCSystem() {
  return (
    "You are Pass C (critic). Compare Pass B claims to Pass A facts and raw aligned moments. " +
    "Downgrade or reject cross_modal inferences when speech and visual timestamps do not support them. " +
    "Reject template phrasing and generic summaries. Keep only claims grounded in evidence. " +
    'Reply raw JSON: {"approved_claims":[{"text":"","evidence":[],"confidence":0-1}],' +
    '"approved_cross_modal":[{"at":"","inference":"","evidence":[],"confidence":0-1}],' +
    '"rejected_claims":[{"text":"","reason":""}],"evidence_gaps":[]}. ' +
    promptLanguageRule()
  );
}

export function chroniclerPassDSystem({ incremental = false } = {}) {
  const mode = incremental
    ? "Update the existing chronicle: weave in new witness since the last version. " +
      "Preserve what still holds; deepen or revise where new evidence changes the story. " +
      "Do not restart from a template or repeat the full day as a fresh report."
    : "Write the chronicle from scratch for this point in the day.";

  return (
    "You are Pass D (folio chronicler). You watched the whole day passively — mic + camera, no interaction. " +
    "Write one intelligent letter about the person's day: flowing prose paragraphs, perceptive analyst voice, " +
    "not a robot summarizer. " +
    mode +
    " Structure by narrative arc (how the day unfolded), not by fixed categories. " +
    "Weave in: what mattered vs noise; cross-modal moments (speech aligned with what was seen); " +
    "decisions vs ideas rejected; implicit threads; open loops for tomorrow; patterns from long_term_memory " +
    "when they genuinely connect (e.g. 'third day in a row…'). Use time naturally ('around 10h', 'by afternoon'). " +
    "Prefer concrete scenes over abstract topic lists. " +
    FORBIDDEN_OUTPUT +
    " " +
    promptLanguageRule() +
    " Start with a single title line: Folio · {localized date}. Then blank line, then prose only."
  );
}

export function sanitizeChronicleProse(prose) {
  let text = String(prose ?? "").trim();
  if (!text) {
    return text;
  }

  const lines = text.split("\n");
  const cleaned = [];

  for (const line of lines) {
    const trimmed = line.trim();
    if (/^#{1,6}\s/.test(trimmed)) {
      const plain = trimmed.replace(/^#{1,6}\s+/, "").trim();
      if (plain && !/^folio\s·/i.test(plain)) {
        cleaned.push(plain);
      }
      continue;
    }
    if (/^[-*•]\s+/.test(trimmed)) {
      cleaned.push(trimmed.replace(/^[-*•]\s+/, "").trim());
      continue;
    }
    if (/^\d+[.)]\s+/.test(trimmed)) {
      cleaned.push(trimmed.replace(/^\d+[.)]\s+/, "").trim());
      continue;
    }
    cleaned.push(line);
  }

  text = cleaned
    .join("\n")
    .replace(/\n{3,}/g, "\n\n")
    .trim();

  return text;
}
