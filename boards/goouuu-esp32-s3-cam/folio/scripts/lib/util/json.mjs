/** Strip line/block comments outside JSON string literals. */
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

  return arrayFallback ?? parsePassBFallback(cleaned);
}

const EMPTY_PASS_B = {
  narrative_arc: "",
  day_phases: [],
  shifts: [],
  cross_modal: [],
  decisions_real: [],
  rejected: [],
  implicit_threads: [],
  open_loops: [],
  patterns: [],
  tomorrow_pull: [],
};

/** Salvage Pass B when JSON is truncated or has markdown inside strings. */
export function parsePassBFallback(text) {
  const src = String(text ?? "").replace(/\*\*/g, "");
  const arcMatch =
    src.match(/"narrative_arc"\s*:\s*"((?:[^"\\]|\\.)*)"/s) ??
    src.match(/"narrative_arc"\s*:\s*"([\s\S]*?)"\s*,\s*"day_phases"/);
  const arc = arcMatch?.[1]?.replace(/\\n/g, "\n").replace(/\s+/g, " ").trim();
  if (!arc) {
    return null;
  }
  return { ...EMPTY_PASS_B, narrative_arc: arc.slice(0, 2000) };
}

/** Minimal Pass C when critic JSON fails — keeps pipeline alive. */
export function parsePassCFallback(text, passB = null) {
  const src = String(text ?? "").replace(/\*\*/g, "").replace(/```json/gi, "").replace(/```/g, "");
  for (const block of candidateBlocks(src).reverse()) {
    const parsed = tryParse(block.trim());
    if (parsed?.approved_claims) {
      return parsed;
    }
  }
  const arc = passB?.narrative_arc?.trim();
  if (!arc) {
    return null;
  }
  return {
    approved_claims: [{ text: arc.slice(0, 800), evidence: [], confidence: 0.55 }],
    approved_cross_modal: [],
    rejected_claims: [],
    evidence_gaps: ["pass C fallback"],
  };
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

  return {
    person_present: bool("person_present"),
    people: num("people"),
    scene: scene || "unknown",
    activity: str("activity", "unknown"),
    objects: [],
    mood: str("mood", "empty"),
    note: str("note"),
  };
}
