export type Tone = "fg" | "muted" | "accent" | "accent2" | "hot" | "cold" | "good" | "bad";

export type Field = {
  label?: string;
  bind?: string;
  text?: string;
  fmt?: string;
  tone?: Tone;
};

export type Screen = {
  id: string;
  title: string;
  tag: string;
  tagBind?: string;
  hero?: Field;
  unit?: string;
  caption?: Field;
  metrics?: Field[];
  rows?: { left: Field; right: Field; icon?: string }[];
  chart?: { bind: string; tone: Tone };
  gauge?: { bind: string; min: number; max: number };
  body?: { bind: string; tone?: Tone };
  list?: Field[];
  icon?: string;
  needle?: string;
  moon?: boolean;
};

export const screens: Screen[] = [
  {
    id: "clock",
    title: "Relogio",
    tag: "agora",
    hero: { bind: "time" },
    caption: { bind: "weekday", tone: "accent" },
    metrics: [
      { label: "DATA", bind: "date", fmt: "%s" },
      { label: "LOCAL", bind: "city", fmt: "%s", tone: "muted" },
    ],
  },
  {
    id: "weather",
    title: "Clima",
    tag: "clima",
    tagBind: "city",
    hero: { bind: "weather.temp", fmt: "%.0f" },
    unit: "°C",
    caption: { bind: "weather.desc" },
    icon: "weather.code",
    metrics: [
      { label: "MIN", bind: "weather.min", fmt: "%.0f", tone: "cold" },
      { label: "UMID", bind: "weather.hum", fmt: "%d%%", tone: "accent" },
      { label: "MAX", bind: "weather.max", fmt: "%.0f", tone: "hot" },
    ],
  },
  {
    id: "forecast",
    title: "Previsao",
    tag: "proximos dias",
    rows: [0, 1, 2].map((i) => ({
      left: { bind: `weather.days.${i}.day`, fmt: "%s", tone: "accent" },
      right: { bind: `weather.days.${i}.max`, fmt: "%.0f" },
      icon: `weather.days.${i}.code`,
    })),
  },
  {
    id: "chart",
    title: "24 horas",
    tag: "24 horas",
    chart: { bind: "weather.hourly", tone: "accent" },
    caption: { bind: "weather.temp", fmt: "agora %.0f°", tone: "muted" },
  },
  {
    id: "rain",
    title: "Chuva",
    tag: "chuva 2h",
    chart: { bind: "weather.rain15", tone: "cold" },
    caption: { bind: "weather.rainin", fmt: "em %d min", tone: "accent" },
    metrics: [{ label: "CHANCE", bind: "weather.rainp", fmt: "%d%%", tone: "cold" }],
  },
  {
    id: "wind",
    title: "Vento",
    tag: "vento",
    hero: { bind: "weather.wind", fmt: "%.0f" },
    unit: "km/h",
    needle: "weather.dir",
    metrics: [
      { label: "RAJADA", bind: "weather.gust", fmt: "%.0f", tone: "hot" },
      { label: "PRESSAO", bind: "weather.pres", fmt: "%.0f", tone: "accent2" },
    ],
  },
  {
    id: "air",
    title: "Ar",
    tag: "qualidade do ar",
    hero: { bind: "air.aqi", fmt: "%d" },
    gauge: { bind: "air.aqi", min: 0, max: 120 },
    caption: { bind: "air.label", tone: "accent" },
    metrics: [
      { label: "PM2.5", bind: "air.pm25", fmt: "%.0f" },
      { label: "UV", bind: "weather.uv", fmt: "%.0f", tone: "hot" },
    ],
  },
  {
    id: "sun",
    title: "Sol",
    tag: "sol",
    hero: { bind: "sun.daylight", fmt: "%s" },
    gauge: { bind: "sun.progress", min: 0, max: 100 },
    metrics: [
      { label: "NASCER", bind: "weather.sunrise", fmt: "%s", tone: "accent" },
      { label: "POR", bind: "weather.sunset", fmt: "%s", tone: "hot" },
    ],
  },
  {
    id: "moon",
    title: "Lua",
    tag: "lua",
    moon: true,
    caption: { bind: "moon.name", tone: "accent" },
    metrics: [{ label: "ILUMINADA", bind: "moon.illum", fmt: "%.0f%%" }],
  },
  {
    id: "news",
    title: "Noticias",
    tag: "brasil",
    list: [
      { bind: "news.0" },
      { bind: "news.1", tone: "muted" },
      { bind: "news.2", tone: "muted" },
    ],
  },
  {
    id: "market",
    title: "Cotacoes",
    tag: "cotacoes",
    rows: [
      { left: { text: "USD", tone: "muted" }, right: { bind: "market.usd", fmt: "%.2f" } },
      { left: { text: "EUR", tone: "muted" }, right: { bind: "market.eur", fmt: "%.2f" } },
      { left: { text: "BTC", tone: "muted" }, right: { bind: "market.btc", fmt: "%.0f" } },
    ],
  },
  {
    id: "rates",
    title: "Taxas",
    tag: "taxas",
    rows: [
      {
        left: { text: "SELIC", tone: "muted" },
        right: { bind: "rates.selic", fmt: "%.2f%%", tone: "accent" },
      },
      {
        left: { text: "CDI", tone: "muted" },
        right: { bind: "rates.cdi", fmt: "%.2f%%", tone: "accent2" },
      },
      {
        left: { text: "IPCA", tone: "muted" },
        right: { bind: "rates.ipca", fmt: "%.2f%%", tone: "hot" },
      },
    ],
  },
  {
    id: "holiday",
    title: "Feriado",
    tag: "proximo feriado",
    hero: { bind: "holiday.daysLeft", fmt: "%d", tone: "accent" },
    unit: "dias",
    body: { bind: "holiday.name" },
    caption: { bind: "holiday.date", tone: "muted" },
  },
  {
    id: "history",
    title: "Neste dia",
    tag: "neste dia",
    hero: { bind: "history.year", fmt: "%d", tone: "accent2" },
    body: { bind: "history.text" },
  },
  {
    id: "space",
    title: "Espaco",
    tag: "espaco",
    hero: { bind: "space.people", fmt: "%d", tone: "accent" },
    unit: "em orbita",
    metrics: [
      { label: "ISS LAT", bind: "space.lat", fmt: "%.0f" },
      { label: "ISS LON", bind: "space.lon", fmt: "%.0f" },
    ],
  },
  {
    id: "dev",
    title: "GitHub",
    tag: "github",
    hero: { bind: "dev.today", fmt: "%d", tone: "good" },
    unit: "commits hoje",
    caption: { bind: "dev.repo", tone: "muted" },
    metrics: [
      { label: "7 DIAS", bind: "dev.week", fmt: "%d", tone: "accent" },
      { label: "REPOS", bind: "dev.repos", fmt: "%d", tone: "accent2" },
      { label: "ATIVO", bind: "dev.activeDays", fmt: "%dd", tone: "good" },
    ],
  },
  {
    id: "ai",
    title: "IA",
    tag: "ia",
    body: { bind: "ai" },
  },
  {
    id: "system",
    title: "Sistema",
    tag: "sistema",
    hero: { bind: "net.ssid", fmt: "%s" },
    caption: { bind: "net.ip", tone: "muted" },
    metrics: [
      { label: "SINAL", bind: "net.rssi", fmt: "%d" },
      { label: "RAM KB", bind: "sys.heap", fmt: "%d", tone: "good" },
      { label: "NO AR", bind: "sys.uptime", fmt: "%s", tone: "muted" },
    ],
  },
];
