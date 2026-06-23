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

/** @deprecated use whisperLanguageCode — kept for logs */
const WHISPER_BY_LOCALE = {
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

function baseLocale(locale) {
  return locale.split("-")[0];
}

export function activeLocale() {
  return CFG.defaultLocale || "pt-BR";
}

export function whisperLanguageCode() {
  if (CFG.whisperLanguage) {
    const w = CFG.whisperLanguage.toLowerCase();
    if (w.length <= 3) {
      return w;
    }
    const fromName = Object.entries(WHISPER_BY_LOCALE).find(([, name]) =>
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

export function whisperLanguage() {
  if (CFG.whisperLanguage && CFG.whisperLanguage.length > 3) {
    return CFG.whisperLanguage;
  }
  const loc = activeLocale();
  return WHISPER_BY_LOCALE[loc] ?? WHISPER_BY_LOCALE[baseLocale(loc)] ?? "English";
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
