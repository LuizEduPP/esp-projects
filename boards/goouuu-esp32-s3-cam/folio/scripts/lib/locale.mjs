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

function baseLocale(locale) {
  return locale.split("-")[0];
}

function localeMeta(locale) {
  return LOCALE_META[locale] ?? LOCALE_META[baseLocale(locale)];
}

export function activeLocale() {
  return CFG.defaultLocale || "pt-BR";
}

export function whisperLanguageCode() {
  if (CFG.sttLanguage) {
    const w = CFG.sttLanguage.toLowerCase();
    if (w.length <= 3) {
      return w;
    }
    const fromName = Object.entries(LOCALE_META).find(([, meta]) =>
      meta.whisperName.toLowerCase().startsWith(w),
    );
    if (fromName) {
      return fromName[1].whisper;
    }
    return CFG.sttLanguage;
  }
  return localeMeta(activeLocale())?.whisper ?? "en";
}

export function promptLanguageName() {
  return localeMeta(activeLocale())?.prompt ?? activeLocale();
}

export function promptLanguageRule() {
  return (
    `Respond in ${promptLanguageName()}. ` +
    `All natural-language fields and captions must use this language. ` +
    `Locale: ${activeLocale()}.`
  );
}
