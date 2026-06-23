import { mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

export function dayFromIso(iso) {
  return iso.slice(0, 10);
}

export function today() {
  const d = new Date();
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const day = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${day}`;
}

export function dayBounds(day) {
  const start = new Date(`${day}T00:00:00`);
  const end = new Date(`${day}T23:59:59.999`);
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

function tryParse(raw) {
  if (!raw) {
    return null;
  }
  const attempts = [
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
    return JSON.parse(repairTruncated(raw));
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

/** Best-effort JSON from LLM output (markdown fences, comments, truncation). */
export function parseJsonLoose(text) {
  const cleaned = String(text ?? "")
    .replace(/\*\*/g, "")
    .replace(/```json/gi, "")
    .replace(/```/g, "");
  const blocks = candidateBlocks(cleaned);
  let arrayFallback = null;

  for (const block of blocks.reverse()) {
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
    return parsed;
  }

  return arrayFallback;
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
