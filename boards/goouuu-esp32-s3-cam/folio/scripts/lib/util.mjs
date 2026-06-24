import { mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

export function dayFromIso(iso) {
  const d = new Date(iso);
  if (Number.isNaN(d.getTime())) {
    return String(iso ?? "").slice(0, 10);
  }
  return localDateIso(d);
}

export function localDateIso(d = new Date()) {
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const day = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${day}`;
}

export function today() {
  return localDateIso(new Date());
}

export function dayBounds(day) {
  const start = new Date(`${day}T00:00:00`);
  const end = new Date(start);
  end.setDate(end.getDate() + 1);
  return { start: start.toISOString(), end: end.toISOString() };
}

export function retentionCutoffIso(days) {
  const d = new Date();
  d.setDate(d.getDate() - days);
  return d.toISOString();
}

export function priorDay(day) {
  return dayOffset(day, -1);
}

export function dayOffset(day, days) {
  const d = new Date(`${day}T12:00:00.000Z`);
  d.setUTCDate(d.getUTCDate() + days);
  return d.toISOString().slice(0, 10);
}

export function isoNow() {
  return new Date().toISOString();
}

export function parseMetaHeader(raw) {
  const out = {};
  if (!raw) {
    return out;
  }
  for (const part of raw.split(";")) {
    const [k, v] = part.split("=");
    if (k && v !== undefined) {
      out[k.trim()] = v.trim();
    }
  }
  return out;
}

const NUM_CHANNELS = 1;
const BITS_PER_SAMPLE = 16;

export function pcmToWav(pcmBuffer, sampleRate) {
  const dataSize = pcmBuffer.length;
  const header = Buffer.alloc(44);
  header.write("RIFF", 0);
  header.writeUInt32LE(36 + dataSize, 4);
  header.write("WAVE", 8);
  header.write("fmt ", 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(NUM_CHANNELS, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE((sampleRate * NUM_CHANNELS * BITS_PER_SAMPLE) / 8, 28);
  header.writeUInt16LE((NUM_CHANNELS * BITS_PER_SAMPLE) / 8, 32);
  header.writeUInt16LE(BITS_PER_SAMPLE, 34);
  header.write("data", 36);
  header.writeUInt32LE(dataSize, 40);
  return Buffer.concat([header, pcmBuffer]);
}

export function writeWav(path, pcmBuffer, sampleRate) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, pcmToWav(pcmBuffer, sampleRate));
}

export function sendJson(res, status, body) {
  res.writeHead(status, { "Content-Type": "application/json" });
  res.end(JSON.stringify(body));
}

export function sendBytes(res, status, body, contentType, cache = "private, max-age=3600") {
  res.writeHead(status, { "Content-Type": contentType, "Cache-Control": cache });
  res.end(body);
}

export function errMsg(err) {
  return [err?.message, err?.cause?.message, err?.cause?.code].filter(Boolean).join(" — ");
}

function stripJsonComments(text) {
  let out = "";
  let inStr = false;
  let esc = false;
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (esc) {
      out += c;
      esc = false;
      continue;
    }
    if (inStr) {
      out += c;
      if (c === "\\") {
        esc = true;
      } else if (c === '"') {
        inStr = false;
      }
      continue;
    }
    if (c === '"') {
      inStr = true;
      out += c;
      continue;
    }
    if (c === "/" && text[i + 1] === "/") {
      while (i < text.length && text[i] !== "\n") {
        i++;
      }
      continue;
    }
    if (c === "/" && text[i + 1] === "*") {
      i += 2;
      while (i < text.length && !(text[i] === "*" && text[i + 1] === "/")) {
        i++;
      }
      i++;
      continue;
    }
    out += c;
  }
  return out;
}

function stripTrailingCommas(text) {
  return text.replace(/,\s*([}\]])/g, "$1");
}

function extractFencedBlock(text) {
  const m = text.match(/```(?:json)?\s*([\s\S]*?)```/i);
  return m ? m[1].trim() : null;
}

/** First balanced {…} or […] from a start index, respecting strings. */
function extractBalanced(text, start) {
  const open = text[start];
  const close = open === "{" ? "}" : open === "[" ? "]" : null;
  if (!close) {
    return null;
  }

  let depth = 0;
  let inStr = false;
  let esc = false;

  for (let i = start; i < text.length; i++) {
    const c = text[i];
    if (esc) {
      esc = false;
      continue;
    }
    if (inStr) {
      if (c === "\\") {
        esc = true;
      } else if (c === '"') {
        inStr = false;
      }
      continue;
    }
    if (c === '"') {
      inStr = true;
      continue;
    }
    if (c === open) {
      depth++;
    } else if (c === close) {
      depth--;
      if (depth === 0) {
        return text.slice(start, i + 1);
      }
    }
  }

  return repairTruncated(text.slice(start), open, close);
}

function repairTruncated(fragment, open = "{", close = "}") {
  let s = stripTrailingCommas(stripJsonComments(fragment.trim()));
  s = s.replace(/,\s*$/, "");

  let inStr = false;
  let esc = false;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (esc) {
      esc = false;
      continue;
    }
    if (inStr) {
      if (c === "\\") {
        esc = true;
      } else if (c === '"') {
        inStr = false;
      }
      continue;
    }
    if (c === '"') {
      inStr = true;
    }
  }
  if (inStr) {
    s += '"';
  }

  const opens = (s.match(new RegExp(`\\${open}`, "g")) || []).length;
  const closes = (s.match(new RegExp(`\\${close}`, "g")) || []).length;
  if (opens > closes) {
    s += close.repeat(opens - closes);
  }

  return s;
}

function normalizeNewlinesInStrings(text) {
  // LM often emits raw newlines inside "key": "...\n..." — collapse to \n escape.
  return text.replace(/"([^"\\]|\\.)*"/gs, (match) =>
    match.replace(/\r\n/g, "\\n").replace(/\n/g, "\\n").replace(/\r/g, "\\n"),
  );
}

function repairJsonKeyTypos(text) {
  // LM typo: "count:" : 1  →  "count": 1
  return text.replace(/"([^"\\]+?):"\s*(?=:)/g, '"$1"');
}

function tryParse(raw) {
  if (!raw) {
    return null;
  }
  const repaired = repairJsonKeyTypos(raw);
  const quoted = quoteBareJsonValues(repaired);
  const attempts = [
    repaired,
    quoted,
    stripTrailingCommas(stripJsonComments(repaired)),
    stripTrailingCommas(stripJsonComments(quoted)),
    stripTrailingCommas(stripJsonComments(normalizeNewlinesInStrings(repaired))),
    stripTrailingCommas(stripJsonComments(normalizeNewlinesInStrings(quoted))),
    raw,
    stripTrailingCommas(stripJsonComments(raw)),
    stripTrailingCommas(stripJsonComments(normalizeNewlinesInStrings(raw))),
  ];
  for (const candidate of attempts) {
    try {
      return JSON.parse(candidate);
    } catch {
      /* next */
    }
  }
  try {
    return JSON.parse(repairTruncated(repairJsonKeyTypos(raw)));
  } catch {
    return null;
  }
}

function candidateBlocks(text) {
  const clean = String(text ?? "").trim();
  const objects = [];
  const arrays = [];

  const fenced = extractFencedBlock(clean);
  if (fenced) {
    objects.push(fenced);
  }

  for (let i = 0; i < clean.length; i++) {
    if (clean[i] === "{") {
      const block = extractBalanced(clean, i);
      if (block) {
        objects.push(block);
      }
    } else if (clean[i] === "[") {
      const block = extractBalanced(clean, i);
      if (block) {
        arrays.push(block);
      }
    }
  }

  const stripped = clean.replace(/```(?:json)?/gi, "").replace(/```/g, "").trim();
  objects.push(stripped);

  // Objects first — avoid matching inner arrays (e.g. "objects": [...]).
  return [...new Set([...objects, ...arrays].filter(Boolean))];
}

const JSON_OBJECT_PRIORITY_KEYS = [
  "summary",
  "unchanged",
  "person_present",
  "people",
  "scene",
  "activity",
  "objects",
  "tags",
  "mood",
  "note",
  "insights",
  "entities",
  "timeline",
  "stats",
  "patterns",
];

function scoreJsonCandidate(value) {
  if (value === null || typeof value !== "object" || Array.isArray(value)) {
    return -1;
  }
  const keys = Object.keys(value);
  if (keys.length <= 3 && keys.every((k) => ["name", "count", "count:", "nome"].includes(k))) {
    return 0;
  }
  let score = keys.length;
  for (const k of JSON_OBJECT_PRIORITY_KEYS) {
    if (k in value) {
      score += 20;
    }
  }
  return score;
}

/** Best-effort JSON from LLM output (markdown fences, comments, truncation). */
export function parseJsonLoose(text) {
  const blocks = candidateBlocks(String(text ?? "").trim());
  let best = null;
  let bestScore = -1;
  let bestLen = -1;
  let arrayFallback = null;

  for (const block of blocks) {
    const parsed = tryParse(block.replace(/```\s*$/g, "").trim());
    if (parsed === null) {
      continue;
    }
    if (Array.isArray(parsed)) {
      if (!arrayFallback) {
        arrayFallback = parsed;
      }
      continue;
    }
    const score = scoreJsonCandidate(parsed);
    const len = block.length;
    if (score > bestScore || (score === bestScore && len > bestLen)) {
      best = parsed;
      bestScore = score;
      bestLen = len;
    }
  }

  return best ?? arrayFallback;
}

/** Regex fallback for frame caption when JSON is too broken. */
export function parseVisionFallback(text) {
  const src = String(text ?? "");
  const str = (key, fallback = "") => {
    const m = src.match(new RegExp(`"${key}"\\s*:\\s*"((?:[^"\\\\]|\\\\.)*)"`, "i"));
    return m ? m[1].replace(/\\n/g, " ").trim() : fallback;
  };
  const num = (key, fallback = 0) => {
    const m = src.match(new RegExp(`"${key}"\\s*:\\s*(\\d+)`, "i"));
    return m ? Number(m[1]) : fallback;
  };
  const bool = (key, fallback = false) => {
    const m = src.match(new RegExp(`"${key}"\\s*:\\s*(true|false)`, "i"));
    return m ? m[1] === "true" : fallback;
  };

  const scene = str("scene");
  if (!scene && !bool("person_present", false) && num("people") === 0) {
    return null;
  }

  const note = str("note");
  const activity = str("activity", "unknown");
  const summary = str("summary") || (scene ? `${scene}. ${activity}. ${note}`.replace(/\.\s*\./g, ".").trim() : note);
  return {
    person_present: bool("person_present"),
    people: num("people"),
    scene: scene || "unknown",
    activity,
    objects: [],
    mood: str("mood", "empty"),
    note,
    summary,
  };
}

function extractLooseString(src, key) {
  const re = new RegExp(`"${key}"\\s*:\\s*"`, "i");
  const m = src.match(re);
  if (!m || m.index == null) {
    return "";
  }
  let i = m.index + m[0].length;
  let out = "";
  while (i < src.length) {
    const c = src[i];
    if (c === "\\") {
      const next = src[i + 1];
      if (next === "n") {
        out += " ";
      } else if (next) {
        out += next;
      }
      i += 2;
      continue;
    }
    if (c === '"') {
      break;
    }
    out += c === "\n" || c === "\r" ? " " : c;
    i++;
  }
  return out.replace(/\s+/g, " ").trim();
}

function extractLooseObjects(src, key) {
  const re = new RegExp(`"${key}"\\s*:\\s*\\[`, "i");
  const m = src.match(re);
  if (!m || m.index == null) {
    return [];
  }
  const start = m.index + m[0].length - 1;
  const block = extractBalanced(src, start);
  if (!block) {
    return [];
  }
  const inner = block.slice(1, -1);
  const out = [];
  for (let i = 0; i < inner.length; i++) {
    if (inner[i] !== "{") {
      continue;
    }
    const obj = extractBalanced(inner, i);
    if (!obj) {
      continue;
    }
    const parsed = tryParse(obj);
    if (parsed && typeof parsed === "object") {
      out.push(parsed);
    }
    i += obj.length - 1;
  }
  return out;
}

function quoteBareJsonValues(text) {
  return text.replace(
    /("(?:evidence|description|pattern|notes)"\s*:\s*)([^"{\[\d\n][^,\}\]]*?)(\s*[,}\]])/gi,
    (_, pre, val, post) => `${pre}${JSON.stringify(String(val).trim())}${post}`,
  );
}

/** Regex fallback when daily insights JSON is broken (multiline strings, bare values). */
export function parseInsightsFallback(text) {
  const src = String(text ?? "")
    .replace(/```(?:json)?/gi, "")
    .replace(/```/g, "")
    .trim();
  const repaired = quoteBareJsonValues(normalizeNewlinesInStrings(src));
  const parsed = tryParse(repaired);
  if (parsed && typeof parsed === "object" && !Array.isArray(parsed) && parsed.summary) {
    return parsed;
  }

  const summary = extractLooseString(src, "summary");
  const insights = extractLooseString(src, "insights");
  const moments = extractLooseObjects(src, "moments");
  const patterns = extractLooseObjects(src, "patterns");
  const entities = extractLooseObjects(src, "entities");

  if (!summary && !insights && !moments.length && !patterns.length && !entities.length) {
    return null;
  }

  return { summary, insights, moments, patterns, entities };
}

const STT_HALLUCINATION_HINTS = [
  "legendas pela comunidade",
  "legendas por ",
  "amara.org",
  "subtitles by",
  "subtitle by",
  "obrigado por assistir",
  "thanks for watching",
  "inscreva-se no canal",
  "subscribe",
  "www.",
  "http://",
  "https://",
];

/** Whisper/LM subtitle boilerplate and junk speech. */
export function isSttHallucination(text, extraPatterns = []) {
  const raw = String(text ?? "").trim();
  if (!raw) {
    return true;
  }
  if (/<\|[^|>]+\|>/.test(raw) || /\bundefined\b/i.test(raw)) {
    return true;
  }
  const lower = raw.toLowerCase();
  const patterns = [...STT_HALLUCINATION_HINTS, ...extraPatterns].filter(Boolean);
  for (const pat of patterns) {
    if (lower.includes(String(pat).toLowerCase())) {
      return true;
    }
  }
  if (/^legendas\b/i.test(raw) && raw.length < 120) {
    return true;
  }
  const norm = lower
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^\p{L}\p{N}\s]/gu, " ")
    .trim();
  const tokens = norm.split(/\s+/).filter(Boolean);
  if (tokens.length >= 3 && new Set(tokens).size === 1) {
    return true;
  }
  return false;
}

/** Dark-room LM caption templates — not useful in the life log. */
export function isDarkSceneCaption(caption) {
  const c = String(caption ?? "").trim();
  if (!c) {
    return false;
  }
  if (/^parece que/i.test(c) && /escuro|dark|sem ilumina|dim|pouco vis|sala sem|ambiente escuro/i.test(c)) {
    return true;
  }
  if (/movimento de uma pessoa ou objeto/i.test(c) && /escuro|dark|sem ilumina/i.test(c)) {
    return true;
  }
  return false;
}
