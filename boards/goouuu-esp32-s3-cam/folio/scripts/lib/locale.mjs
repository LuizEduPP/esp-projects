import { CFG } from "./config.mjs";

const LOCALE_META = {
  "pt-BR": { whisper: "pt", whisperName: "Portuguese", prompt: "Portuguese (Brazil)" },
  "pt-PT": { whisper: "pt", whisperName: "Portuguese", prompt: "Portuguese (Portugal)" },
  pt: { whisper: "pt", whisperName: "Portuguese", prompt: "Portuguese" },
  "en-US": { whisper: "en", whisperName: "English", prompt: "English (US)" },
  "en-GB": { whisper: "en", whisperName: "English", prompt: "English (UK)" },
  en: { whisper: "en", whisperName: "English", prompt: "English" },
  "es-ES": { whisper: "es", whisperName: "Spanish", prompt: "Spanish" },
  es: { whisper: "es", whisperName: "Spanish", prompt: "Spanish" },
  "fr-FR": { whisper: "fr", whisperName: "French", prompt: "French" },
  fr: { whisper: "fr", whisperName: "French", prompt: "French" },
  "de-DE": { whisper: "de", whisperName: "German", prompt: "German" },
  de: { whisper: "de", whisperName: "German", prompt: "German" },
  "it-IT": { whisper: "it", whisperName: "Italian", prompt: "Italian" },
  it: { whisper: "it", whisperName: "Italian", prompt: "Italian" },
  "ja-JP": { whisper: "ja", whisperName: "Japanese", prompt: "Japanese" },
  ja: { whisper: "ja", whisperName: "Japanese", prompt: "Japanese" },
  "zh-CN": { whisper: "zh", whisperName: "Chinese", prompt: "Chinese (Simplified)" },
  zh: { whisper: "zh", whisperName: "Chinese", prompt: "Chinese" },
};

const EVIDENCE_FOOTER_BY_BASE = {
  pt: "End with one short italic line: Evidência: comma-separated evidence IDs used.",
  es: "End with one short italic line: Evidencia: comma-separated evidence IDs used.",
  fr: "End with one short italic line: Preuves : comma-separated evidence IDs used.",
  de: "End with one short italic line: Belege: comma-separated evidence IDs used.",
};

export const LEXICAL_MIN_TOKEN_LENGTH = 3;

const LEXICAL_STOP_WORDS_PT = [
  "a", "o", "e", "de", "da", "do", "das", "dos", "em", "no", "na", "nos", "nas",
  "um", "uma", "uns", "umas", "para", "por", "com", "sem", "que", "se", "as", "os",
];

const LEXICAL_STOP_WORDS_EN = [
  "the", "and", "or", "of", "to", "in", "on", "at", "is", "it",
];

function baseLocale(locale) {
  return locale.split("-")[0];
}

function localeMeta(locale) {
  return LOCALE_META[locale] ?? LOCALE_META[baseLocale(locale)];
}

export function activeLocale() {
  return CFG.defaultLocale || "pt-BR";
}

export function lexicalStopWordSet() {
  const base = baseLocale(activeLocale());
  const primary = base === "pt" ? LEXICAL_STOP_WORDS_PT : LEXICAL_STOP_WORDS_EN;
  const extra = base === "pt" ? LEXICAL_STOP_WORDS_EN : [];
  return new Set([...primary, ...extra]);
}

export function whisperLanguageCode() {
  if (CFG.whisperLanguage) {
    const w = CFG.whisperLanguage.toLowerCase();
    if (w.length <= 3) {
      return w;
    }
    const fromName = Object.entries(LOCALE_META).find(([, meta]) =>
      meta.whisperName.toLowerCase().startsWith(w),
    );
    if (fromName) {
      return fromName[1].whisper;
    }
    return CFG.whisperLanguage;
  }
  return localeMeta(activeLocale())?.whisper ?? "en";
}

export function promptLanguageName() {
  return localeMeta(activeLocale())?.prompt ?? activeLocale();
}

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
  const base = baseLocale(activeLocale());
  return (
    EVIDENCE_FOOTER_BY_BASE[base] ??
    "End with one short italic line: Evidence: comma-separated evidence IDs used."
  );
}
