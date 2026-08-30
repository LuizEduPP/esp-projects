import type { Item, Page, Theme } from "./types.js";

const W = 128;
const MARGIN = 8;
const INNER = W - MARGIN * 2;

const label = (
  t: Theme,
  opts: {
    x?: number;
    y: number;
    w?: number;
    font?: number;
    color?: string;
    align?: "left" | "center" | "right";
    text?: string;
    bind?: string;
    fmt?: string;
    wrap?: boolean;
    h?: number;
    scroll?: boolean;
  },
): Item => ({
  t: "label",
  x: opts.x ?? MARGIN,
  y: opts.y,
  w: opts.w ?? INNER,
  font: opts.font ?? t.body,
  color: opts.color ?? t.fg,
  align: opts.align ?? "center",
  ...(opts.text !== undefined ? { text: opts.text } : {}),
  ...(opts.bind !== undefined ? { bind: opts.bind } : {}),
  ...(opts.fmt !== undefined ? { fmt: opts.fmt } : {}),
  ...(opts.wrap ? { wrap: true } : {}),
  ...(opts.h !== undefined ? { h: opts.h } : {}),
  ...(opts.scroll ? { scroll: true } : {}),
});

const tag = (t: Theme, text: string, y = 8): Item =>
  label(t, { y, font: t.tag, color: t.muted, text: t.upper ? text.toUpperCase() : text });

const plate = (t: Theme, x: number, y: number, w: number, h: number): Item => ({
  t: "plate",
  x,
  y,
  w,
  h,
  style: t.plate,
  color: t.plateColor,
});

const rule = (t: Theme, x: number, y: number, w: number, h = 1): Item => ({
  t: "rule",
  x,
  y,
  w,
  h,
  color: t.line,
});

const trio = (
  t: Theme,
  y: number,
  cells: { label: string; bind: string; fmt: string; color: string }[],
): Item[] => {
  const items: Item[] = [plate(t, MARGIN, y, INNER, 34)];
  const step = INNER / cells.length;

  cells.forEach((cell, i) => {
    const x = MARGIN + step * i;
    items.push(label(t, { x, y: y + 5, w: step, font: t.tag, color: t.muted, text: cell.label }));
    items.push(
      label(t, { x, y: y + 16, w: step, font: t.body, color: cell.color, bind: cell.bind, fmt: cell.fmt }),
    );
    if (i > 0) items.push(rule(t, Math.round(x), y + 6, 1, 22));
  });

  return items;
};

export const buildPages = (t: Theme): Page[] => {
  const bg = t.bg;

  return [
    {
      id: "clock",
      title: "Relogio",
      bg,
      items: [
        label(t, { y: 10, font: t.tag, color: t.muted, bind: "city", scroll: true }),
        label(t, { y: 30, font: t.hero, color: t.fg, bind: "time" }),
        label(t, { y: 72, font: t.tag, color: t.accent, bind: "weekday" }),
        rule(t, 44, 88, 40),
        label(t, { y: 96, font: t.body, color: t.muted, bind: "date" }),
      ],
    },
    {
      id: "weather",
      title: "Clima",
      bg,
      items: [
        tag(t, "clima"),
        { t: "icon", x: 6, y: 24, size: 40, color: t.accent, color2: t.muted, bind: "weather.code" },
        label(t, {
          x: 50,
          y: 26,
          w: 72,
          font: t.hero,
          color: t.fg,
          bind: "weather.temp",
          fmt: "%.0f",
        }),
        label(t, { x: 50, y: 58, w: 72, font: t.tag, color: t.muted, text: "graus" }),
        label(t, { y: 76, font: t.body, color: t.muted, bind: "weather.desc", scroll: true }),
        ...trio(t, 92, [
          { label: "MIN", bind: "weather.min", fmt: "%.0f", color: t.cold },
          { label: "UMID", bind: "weather.hum", fmt: "%d%%", color: t.accent },
          { label: "MAX", bind: "weather.max", fmt: "%.0f", color: t.hot },
        ]),
      ],
    },
    {
      id: "forecast",
      title: "Previsao",
      bg,
      items: [
        tag(t, "proximos dias"),
        ...[0, 1, 2].flatMap((i): Item[] => {
          const y = 24 + i * 34;
          return [
            plate(t, MARGIN, y, INNER, 30),
            label(t, {
              x: MARGIN + 6,
              y: y + 9,
              w: 34,
              font: t.body,
              color: t.accent,
              align: "left",
              bind: `weather.days.${i}.day`,
            }),
            { t: "icon", x: 52, y: y + 5, size: 20, color: t.fg, color2: t.muted, bind: `weather.days.${i}.code` },
            label(t, {
              x: 76,
              y: y + 9,
              w: 40,
              font: t.body,
              color: t.fg,
              align: "right",
              bind: `weather.days.${i}.max`,
              fmt: "%.0f",
            }),
          ];
        }),
      ],
    },
    {
      id: "chart",
      title: "24 horas",
      bg,
      items: [
        tag(t, "24 horas"),
        { t: "chart", x: MARGIN, y: 26, w: INNER, h: 62, color: t.accent, bind: "weather.hourly" },
        rule(t, MARGIN, 90, INNER),
        label(t, {
          y: 98,
          font: t.tag,
          color: t.muted,
          bind: "weather.temp",
          fmt: "agora %.0f graus",
        }),
      ],
    },
    {
      id: "rain",
      title: "Chuva",
      bg,
      items: [
        tag(t, "chuva 2h"),
        { t: "chart", x: MARGIN, y: 30, w: INNER, h: 54, color: t.cold, bind: "weather.rain15" },
        rule(t, MARGIN, 86, INNER),
        label(t, { y: 94, font: t.body, color: t.accent, bind: "weather.rainin", fmt: "em %d min" }),
        label(t, { y: 110, font: t.tag, color: t.muted, bind: "weather.rainp", fmt: "chance %d%%" }),
      ],
    },
    {
      id: "wind",
      title: "Vento",
      bg,
      items: [
        tag(t, "vento"),
        { t: "needle", x: 34, y: 22, size: 60, color: t.accent, track: t.line, bind: "weather.dir" },
        label(t, { y: 84, font: t.big, color: t.fg, bind: "weather.wind", fmt: "%.0f km/h" }),
        ...trio(t, 96, [
          { label: "RAJADA", bind: "weather.gust", fmt: "%.0f", color: t.hot },
          { label: "PRESSAO", bind: "weather.pres", fmt: "%.0f", color: t.accent2 },
        ]),
      ],
    },
    {
      id: "air",
      title: "Ar",
      bg,
      items: [
        tag(t, "qualidade do ar"),
        { t: "arc", x: 30, y: 20, size: 68, width: 8, color: t.accent, track: t.line, bind: "air.aqi", min: 0, max: 120 },
        label(t, { x: 30, y: 44, w: 68, font: t.big, color: t.fg, bind: "air.aqi", fmt: "%d" }),
        label(t, { y: 92, font: t.body, color: t.accent, bind: "air.label" }),
        label(t, { y: 108, font: t.tag, color: t.muted, bind: "air.pm25", fmt: "PM2.5 %.0f" }),
      ],
    },
    {
      id: "sun",
      title: "Sol",
      bg,
      items: [
        tag(t, "sol"),
        { t: "arc", x: 24, y: 22, size: 80, width: 6, color: t.accent, track: t.line, bind: "sun.progress", min: 0, max: 100 },
        label(t, { x: 24, y: 48, w: 80, font: t.body, color: t.fg, bind: "sun.daylight" }),
        ...trio(t, 96, [
          { label: "NASCER", bind: "weather.sunrise", fmt: "%s", color: t.accent },
          { label: "POR", bind: "weather.sunset", fmt: "%s", color: t.hot },
        ]),
      ],
    },
    {
      id: "moon",
      title: "Lua",
      bg,
      items: [
        tag(t, "lua"),
        { t: "moon", x: 40, y: 22, size: 48, color: t.fg },
        label(t, { y: 78, font: t.body, color: t.accent, bind: "moon.name" }),
        label(t, { y: 96, font: t.tag, color: t.muted, bind: "moon.illum", fmt: "%.0f%% iluminada" }),
      ],
    },
    {
      id: "news",
      title: "Noticias",
      bg,
      items: [
        tag(t, "brasil"),
        ...[0, 1, 2].flatMap((i): Item[] => {
          const y = 24 + i * 33;
          const items: Item[] = [
            label(t, {
              y,
              font: t.tag,
              color: i === 0 ? t.fg : t.muted,
              bind: `news.${i}`,
              wrap: true,
              h: 29,
            }),
          ];
          if (i < 2) items.push(rule(t, 34, y + 30, 60));
          return items;
        }),
      ],
    },
    {
      id: "market",
      title: "Cotacoes",
      bg,
      items: [
        tag(t, "cotacoes"),
        ...["usd", "eur", "btc"].flatMap((sym, i): Item[] => {
          const y = 26 + i * 30;
          return [
            plate(t, MARGIN, y, INNER, 26),
            label(t, {
              x: MARGIN + 6,
              y: y + 8,
              w: 34,
              font: t.tag,
              color: t.muted,
              align: "left",
              text: sym.toUpperCase(),
            }),
            label(t, {
              x: 50,
              y: y + 6,
              w: 64,
              font: t.body,
              color: t.fg,
              align: "right",
              bind: `market.${sym}`,
              fmt: sym === "btc" ? "%.0f" : "%.2f",
            }),
          ];
        }),
      ],
    },
    {
      id: "rates",
      title: "Taxas",
      bg,
      items: [
        tag(t, "taxas"),
        ...["selic", "cdi", "ipca"].flatMap((name, i): Item[] => {
          const y = 26 + i * 30;
          return [
            plate(t, MARGIN, y, INNER, 26),
            label(t, {
              x: MARGIN + 6,
              y: y + 8,
              w: 40,
              font: t.tag,
              color: t.muted,
              align: "left",
              text: name.toUpperCase(),
            }),
            label(t, {
              x: 56,
              y: y + 6,
              w: 58,
              font: t.body,
              color: [t.accent, t.accent2, t.hot][i] ?? t.fg,
              align: "right",
              bind: `rates.${name}`,
              fmt: "%.2f%%",
            }),
          ];
        }),
      ],
    },
    {
      id: "holiday",
      title: "Feriado",
      bg,
      items: [
        tag(t, "proximo feriado"),
        label(t, { y: 26, font: t.hero, color: t.accent, bind: "holiday.daysLeft", fmt: "%d" }),
        label(t, { y: 62, font: t.tag, color: t.muted, text: "dias" }),
        label(t, { y: 78, font: t.body, color: t.fg, bind: "holiday.name", wrap: true, h: 28 }),
        label(t, { y: 110, font: t.tag, color: t.muted, bind: "holiday.date" }),
      ],
    },
    {
      id: "history",
      title: "Neste dia",
      bg,
      items: [
        tag(t, "neste dia"),
        label(t, { y: 22, font: t.big, color: t.accent2, bind: "history.year", fmt: "%d" }),
        plate(t, MARGIN, 46, INNER, 66),
        label(t, {
          x: MARGIN + 6,
          y: 52,
          w: INNER - 12,
          font: t.tag,
          color: t.fg,
          bind: "history.text",
          wrap: true,
          h: 54,
        }),
      ],
    },
    {
      id: "space",
      title: "Espaco",
      bg,
      items: [
        tag(t, "espaco"),
        label(t, { y: 26, font: t.hero, color: t.accent, bind: "space.people", fmt: "%d" }),
        label(t, { y: 64, font: t.tag, color: t.muted, text: "pessoas em orbita" }),
        ...trio(t, 84, [
          { label: "ISS LAT", bind: "space.lat", fmt: "%.0f", color: t.fg },
          { label: "ISS LON", bind: "space.lon", fmt: "%.0f", color: t.fg },
        ]),
      ],
    },
    {
      id: "dev",
      title: "GitHub",
      bg,
      items: [
        tag(t, "github"),
        label(t, { y: 22, font: t.hero, color: t.good, bind: "dev.today", fmt: "%d" }),
        label(t, { y: 58, font: t.tag, color: t.muted, text: "commits hoje" }),
        ...trio(t, 74, [
          { label: "7 DIAS", bind: "dev.week", fmt: "%d", color: t.accent },
          { label: "REPOS", bind: "dev.repos", fmt: "%d", color: t.accent2 },
          { label: "ATIVO", bind: "dev.activeDays", fmt: "%dd", color: t.good },
        ]),
        label(t, { y: 112, font: t.tag, color: t.muted, bind: "dev.repo", scroll: true }),
      ],
    },
    {
      id: "timer",
      title: "Timer",
      bg,
      items: [
        tag(t, "pomodoro"),
        { t: "arc", x: 24, y: 22, size: 80, width: 7, color: t.accent, track: t.line, bind: "timer.percent", min: 0, max: 100 },
        label(t, { x: 24, y: 48, w: 80, font: t.big, color: t.fg, bind: "timer.clock" }),
        label(t, { y: 108, font: t.tag, color: t.muted, bind: "timer.mode" }),
      ],
    },
    {
      id: "ai",
      title: "IA",
      bg,
      items: [
        tag(t, "ia"),
        plate(t, MARGIN, 22, INNER, 82),
        label(t, {
          x: MARGIN + 5,
          y: 28,
          w: INNER - 10,
          font: t.body,
          color: t.fg,
          bind: "ai",
          wrap: true,
          h: 70,
        }),
      ],
    },
    {
      id: "system",
      title: "Sistema",
      bg,
      items: [
        tag(t, "sistema"),
        label(t, { y: 24, font: t.body, color: t.accent, bind: "net.ssid", scroll: true }),
        label(t, { y: 42, font: t.tag, color: t.muted, bind: "net.ip" }),
        ...trio(t, 60, [
          { label: "SINAL", bind: "net.rssi", fmt: "%d", color: t.fg },
          { label: "RAM KB", bind: "sys.heap", fmt: "%d", color: t.good },
        ]),
        label(t, { y: 102, font: t.tag, color: t.muted, bind: "sys.uptime", fmt: "no ar ha %s" }),
      ],
    },
  ];
};
