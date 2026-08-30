const num = (name: string, fallback: number): number => {
  const raw = process.env[name];
  const parsed = raw === undefined ? NaN : Number(raw);
  return Number.isFinite(parsed) ? parsed : fallback;
};

const str = (name: string, fallback: string): string => process.env[name]?.trim() || fallback;

export const config = {
  port: num("PORT", 8090),
  host: str("HOST", "0.0.0.0"),

  city: str("DASH_CITY", "Cesario Lange"),
  lat: num("DASH_LAT", -23.2267),
  lon: num("DASH_LON", -47.9531),
  timezone: str("DASH_TIMEZONE", "America/Sao_Paulo"),

  githubUser: str("DASH_GITHUB_USER", "LuizEduPP"),
  githubToken: str("DASH_GITHUB_TOKEN", ""),

  ollamaUrl: str("DASH_OLLAMA_URL", "https://ollama.kmali.online/api/chat"),
  ollamaModel: str("DASH_OLLAMA_MODEL", "gemma4:e4b"),

  newsUrl: str("DASH_NEWS_URL", "https://agenciabrasil.ebc.com.br/rss/ultimasnoticias/feed.xml"),

  otaToken: str("DASH_OTA_TOKEN", ""),
} as const;
