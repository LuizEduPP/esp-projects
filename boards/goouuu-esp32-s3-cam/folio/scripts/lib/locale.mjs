import { CFG } from "./config.mjs";

/** Whisper CLI --language (ISO 639-1 codes). */
const WHISPER_CODE_BY_LOCALE = {
  "pt-BR": "pt",
  "pt-PT": "pt",
  pt: "pt",
  "en-US": "en",
  "en-GB": "en",
  en: "en",
  "es-ES": "es",
  es: "es",
  "fr-FR": "fr",
  fr: "fr",
  "de-DE": "de",
  de: "de",
  "it-IT": "it",
  it: "it",
  "ja-JP": "ja",
  ja: "ja",
  "zh-CN": "zh",
  zh: "zh",
};

/** Human-readable Whisper language names for FOLIO_WHISPER_LANGUAGE lookup. */
const WHISPER_NAME_BY_LOCALE = {
  "pt-BR": "Portuguese",
  "pt-PT": "Portuguese",
  pt: "Portuguese",
  "en-US": "English",
  "en-GB": "English",
  en: "English",
  "es-ES": "Spanish",
  es: "Spanish",
  "fr-FR": "French",
  fr: "French",
  "de-DE": "German",
  de: "German",
  "it-IT": "Italian",
  it: "Italian",
  "ja-JP": "Japanese",
  ja: "Japanese",
  "zh-CN": "Chinese",
  zh: "Chinese",
};

/** Human label for LM prompts */
const PROMPT_LANGUAGE = {
  "pt-BR": "Portuguese (Brazil)",
  "pt-PT": "Portuguese (Portugal)",
  pt: "Portuguese",
  "en-US": "English (US)",
  "en-GB": "English (UK)",
  en: "English",
  "es-ES": "Spanish",
  es: "Spanish",
  "fr-FR": "French",
  fr: "French",
  "de-DE": "German",
  de: "German",
  "it-IT": "Italian",
  it: "Italian",
  "ja-JP": "Japanese",
  ja: "Japanese",
  "zh-CN": "Chinese (Simplified)",
  zh: "Chinese",
};

/** Minimum token length for lexical RAG (tokens shorter than this are dropped). */
export const LEXICAL_MIN_TOKEN_LENGTH = 3;

const LEXICAL_STOP_WORDS_PT = [
  "a",
  "o",
  "e",
  "de",
  "da",
  "do",
  "das",
  "dos",
  "em",
  "no",
  "na",
  "nos",
  "nas",
  "um",
  "uma",
  "uns",
  "umas",
  "para",
  "por",
  "com",
  "sem",
  "que",
  "se",
  "as",
  "os",
];

const LEXICAL_STOP_WORDS_EN = [
  "the",
  "and",
  "or",
  "of",
  "to",
  "in",
  "on",
  "at",
  "is",
  "it",
];

const LEXICAL_STOP_WORDS_BY_BASE = {
  pt: LEXICAL_STOP_WORDS_PT,
  en: LEXICAL_STOP_WORDS_EN,
};

function baseLocale(locale) {
  return locale.split("-")[0];
}

export function activeLocale() {
  return CFG.defaultLocale || "pt-BR";
}

/** Stop words for lexical memory retrieval, keyed off active locale. */
export function lexicalStopWordSet() {
  const base = baseLocale(activeLocale());
  const primary = LEXICAL_STOP_WORDS_BY_BASE[base] ?? LEXICAL_STOP_WORDS_BY_BASE.en;
  const extra = base === "pt" ? LEXICAL_STOP_WORDS_EN : [];
  return new Set([...primary, ...extra]);
}

export function whisperLanguageCode() {
  if (CFG.whisperLanguage) {
    const w = CFG.whisperLanguage.toLowerCase();
    if (w.length <= 3) {
      return w;
    }
    const fromName = Object.entries(WHISPER_NAME_BY_LOCALE).find(([, name]) =>
      name.toLowerCase().startsWith(w),
    );
    if (fromName) {
      return WHISPER_CODE_BY_LOCALE[fromName[0]] ?? fromName[0];
    }
    return CFG.whisperLanguage;
  }
  const loc = activeLocale();
  return WHISPER_CODE_BY_LOCALE[loc] ?? WHISPER_CODE_BY_LOCALE[baseLocale(loc)] ?? "en";
}

export function promptLanguageName() {
  const loc = activeLocale();
  return PROMPT_LANGUAGE[loc] ?? PROMPT_LANGUAGE[baseLocale(loc)] ?? loc;
}

/** Appended to every LM system/user prompt that emits natural language. */
export function promptLanguageRule() {
  return (
    `Respond in ${promptLanguageName()}. ` +
    `All natural-language fields, captions, facts, and prose must use this language. ` +
    `Locale: ${activeLocale()}.`
  );
}

export function dateLabelForDay(day) {
  return new Date(`${day}T12:00:00.000Z`).toLocaleDateString(activeLocale(), {
    weekday: "long",
    day: "numeric",
    month: "long",
  });
}

export function evidenceFooterRule() {
  const loc = activeLocale();
  const base = baseLocale(loc);
  if (base === "pt") {
    return "End with one short italic line: Evidência: comma-separated evidence IDs used.";
  }
  if (base === "es") {
    return "End with one short italic line: Evidencia: comma-separated evidence IDs used.";
  }
  if (base === "fr") {
    return "End with one short italic line: Preuves : comma-separated evidence IDs used.";
  }
  if (base === "de") {
    return "End with one short italic line: Belege: comma-separated evidence IDs used.";
  }
  return "End with one short italic line: Evidence: comma-separated evidence IDs used.";
}
