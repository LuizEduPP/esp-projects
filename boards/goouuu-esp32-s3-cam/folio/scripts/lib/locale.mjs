import { CFG } from "./config.mjs";

/** Whisper CLI --language values */
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

export function whisperLanguage() {
  if (CFG.whisperLanguage) {
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
