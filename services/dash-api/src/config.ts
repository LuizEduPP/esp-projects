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

  aiUrl: str("DASH_AI_URL", "https://ai-gateway.kmali.online/api/chat"),
  aiModel: str("DASH_AI_MODEL", "lfm2.5-thinking:latest"),
  aiToken: str("DASH_AI_TOKEN", ""),

  newsUrl: str("DASH_NEWS_URL", "https://agenciabrasil.ebc.com.br/rss/ultimasnoticias/feed.xml"),

  otaToken: str("DASH_OTA_TOKEN", ""),
} as const;
