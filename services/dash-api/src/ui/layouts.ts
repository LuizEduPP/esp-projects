import type { Field, Screen, Tone } from "./content.js";
import type { Item, Theme } from "./types.js";

const W = 128;
const H = 128;
const M = 8;
const INNER = W - M * 2;

const tone = (t: Theme, name: Tone = "fg"): string => t[name];

// Herois de texto (relogio, SSID, duracao) nao cabem na fonte gigante.
const isText = (f?: Field): boolean =>
  !!f && (f.fmt === "%s" || !f.fmt || /time|clock|daylight|ssid/.test(f.bind ?? ""));

type Txt = {
  x?: number;
  y: number;
  w?: number;
  font: number;
  color: string;
  align?: "left" | "center" | "right";
  field?: Field;
  text?: string;
  wrap?: boolean;
  h?: number;
  scroll?: boolean;
};

const txt = (o: Txt): Item => ({
  t: "label",
  x: o.x ?? M,
  y: o.y,
  w: o.w ?? INNER,
  font: o.font,
  color: o.color,
  align: o.align ?? "center",
  ...(o.field?.bind ? { bind: o.field.bind } : {}),
  ...(o.field?.fmt ? { fmt: o.field.fmt } : {}),
  ...(o.text !== undefined ? { text: o.text } : o.field?.text ? { text: o.field.text } : {}),
  ...(o.wrap ? { wrap: true } : {}),
  ...(o.h !== undefined ? { h: o.h } : {}),
  ...(o.scroll ? { scroll: true } : {}),
});

const tag = (t: Theme, s: Screen, y = 8, x = M, w = INNER, align?: "left" | "center"): Item =>
  txt({
    x,
    y,
    w,
    align,
    font: t.tag,
    color: t.muted,
    text: t.upper ? s.tag.toUpperCase() : s.tag,
  });

const plate = (t: Theme, x: number, y: number, w: number, h: number, color?: string): Item => ({
  t: "plate",
  x,
  y,
  w,
  h,
  style: t.plate,
  color: color ?? t.plateColor,
});

const solid = (x: number, y: number, w: number, h: number, color: string): Item => ({
  t: "plate",
  x,
  y,
  w,
  h,
  style: "solid",
  color,
});

const rule = (t: Theme, x: number, y: number, w: number, h = 1, color?: string): Item => ({
  t: "rule",
  x,
  y,
  w,
  h,
  color: color ?? t.line,
});

// --- blocos compartilhados -------------------------------------------------
// Telas sem heroi (previsao, cotacoes, noticias, graficos, textos) usam estas
// pecas em qualquer familia, coloridas pelos tokens do tema.

const chartBlock = (t: Theme, s: Screen, y: number, h: number): Item[] => [
  { t: "chart", x: M, y, w: INNER, h, color: tone(t, s.chart!.tone), bind: s.chart!.bind },
];

const bodyBlock = (t: Theme, s: Screen, y: number, h: number): Item[] => [
  ...(t.plate === "none" ? [] : [plate(t, M, y, INNER, h)]),
  txt({
    x: M + 5,
    y: y + 5,
    w: INNER - 10,
    font: t.body,
    color: tone(t, s.body!.tone),
    field: s.body,
    wrap: true,
    h: h - 10,
  }),
];

const listBlock = (t: Theme, s: Screen, y: number): Item[] => {
  const items: Item[] = [];
  const step = (H - y - 6) / s.list!.length;

  s.list!.forEach((field, i) => {
    const top = y + step * i;
    items.push(
      txt({
        y: top,
        font: t.tag,
        color: tone(t, field.tone ?? "fg"),
        field,
        wrap: true,
        h: step - 4,
      }),
    );
    if (i < s.list!.length - 1) items.push(rule(t, 34, top + step - 4, 60));
  });

  return items;
};

const rowsBlock = (t: Theme, s: Screen, y: number, boxed: boolean): Item[] => {
  const items: Item[] = [];
  const rows = s.rows!;
  const step = Math.min(30, (H - y - 6) / rows.length);

  rows.forEach((row, i) => {
    const top = y + step * i;
    const h = step - 4;

    if (boxed) {
      items.push(plate(t, M, top, INNER, h));
    } else if (i < rows.length - 1) {
      items.push(rule(t, M, top + h, INNER));
    }

    items.push(
      txt({
        x: M + 6,
        y: top + (h - t.tag) / 2 - 1,
        w: 44,
        align: "left",
        font: t.tag,
        color: tone(t, row.left.tone ?? "muted"),
        field: row.left,
      }),
    );

    if (row.icon) {
      items.push({
        t: "icon",
        x: 56,
        y: top + 3,
        size: h - 6,
        color: t.accent,
        color2: t.muted,
        bind: row.icon,
      });
    }

    items.push(
      txt({
        x: 74,
        y: top + (h - t.body) / 2 - 2,
        w: 46,
        align: "right",
        font: t.body,
        color: tone(t, row.right.tone ?? "fg"),
        field: row.right,
      }),
    );
  });

  return items;
};

// Trio de metricas numa faixa horizontal, o padrao das familias centradas.
const metricStrip = (t: Theme, s: Screen, y: number, h = 34): Item[] => {
  const cells = s.metrics!;
  const items: Item[] = t.plate === "none" ? [] : [plate(t, M, y, INNER, h)];
  const step = INNER / cells.length;

  cells.forEach((cell, i) => {
    const x = M + step * i;
    items.push(
      txt({ x, y: y + 5, w: step, font: t.tag, color: t.muted, text: cell.label ?? "" }),
    );
    items.push(
      txt({ x, y: y + 16, w: step, font: t.body, color: tone(t, cell.tone ?? "fg"), field: cell }),
    );
    if (i > 0) items.push(rule(t, Math.round(x), y + 6, 1, h - 12));
  });

  return items;
};

// --- familias --------------------------------------------------------------

type Layout = (t: Theme, s: Screen) => Item[];

// 01 Heroi centrado: tag, valor grande no meio, legenda, faixa de metricas.
const hero: Layout = (t, s) => {
  const items: Item[] = [tag(t, s)];

  if (s.chart) {
    items.push(...chartBlock(t, s, 26, 62));
    if (s.caption) items.push(txt({ y: 96, font: t.tag, color: t.muted, field: s.caption }));
    if (s.metrics) items.push(...metricStrip(t, s, 108, 18));
    return items;
  }

  if (s.rows) return [...items, ...rowsBlock(t, s, 24, t.plate !== "none")];
  if (s.list) return [...items, ...listBlock(t, s, 24)];

  let y = 22;

  if (s.gauge) {
    items.push({
      t: "arc",
      x: 24,
      y: 20,
      size: 80,
      width: 7,
      color: t.accent,
      track: t.line,
      bind: s.gauge.bind,
      min: s.gauge.min,
      max: s.gauge.max,
    });
    if (s.hero) {
      items.push(
        txt({
          x: 24,
          y: 46,
          w: 80,
          font: isText(s.hero) ? t.big : t.hero,
          color: tone(t, s.hero.tone ?? "fg"),
          field: s.hero,
        }),
      );
    }
    y = 104;
  } else if (s.moon) {
    items.push({ t: "moon", x: 40, y: 22, size: 48, color: t.fg });
    y = 76;
  } else if (s.needle) {
    items.push({
      t: "needle",
      x: 34,
      y: 20,
      size: 58,
      color: t.accent,
      track: t.line,
      bind: s.needle,
    });
    if (s.hero) {
      items.push(txt({ y: 80, font: t.big, color: tone(t, s.hero.tone ?? "fg"), field: s.hero }));
    }
    y = 100;
  } else if (s.hero) {
    if (s.icon) {
      items.push({
        t: "icon",
        x: 6,
        y: 24,
        size: 40,
        color: t.accent,
        color2: t.muted,
        bind: s.icon,
      });
      items.push(
        txt({
          x: 48,
          y: 24,
          w: 74,
          font: t.hero,
          color: tone(t, s.hero.tone ?? "fg"),
          field: s.hero,
        }),
      );
      if (s.unit) items.push(txt({ x: 48, y: 56, w: 74, font: t.tag, color: t.muted, text: s.unit }));
    } else {
      const font = isText(s.hero) ? t.big : t.hero;
      items.push(txt({ y: 24, font, color: tone(t, s.hero.tone ?? "fg"), field: s.hero }));
      if (s.unit) items.push(txt({ y: 24 + font + 6, font: t.tag, color: t.muted, text: s.unit }));
    }
    y = 68;
  }

  if (s.body) {
    items.push(...bodyBlock(t, s, y, H - y - (s.caption ? 20 : 6)));
    if (s.caption) {
      items.push(txt({ y: H - 16, font: t.tag, color: t.muted, field: s.caption, scroll: true }));
    }
    return items;
  }

  if (s.caption) {
    items.push(txt({ y, font: t.body, color: tone(t, s.caption.tone ?? "fg"), field: s.caption, scroll: true }));
    y += t.body + 8;
  }

  if (s.metrics) items.push(...metricStrip(t, s, Math.max(y, H - 40)));
  return items;
};

// 02 Bento: blocos de tamanhos diferentes, sem espaco morto.
const bento: Layout = (t, s) => {
  if (s.rows) return [tag(t, s, 6), ...rowsBlock(t, s, 22, true)];
  if (s.list) return [tag(t, s, 6), ...listBlock(t, s, 22)];
  if (s.chart) {
    return [
      solid(5, 5, 118, 30, t.plateColor),
      txt({ x: 11, y: 13, w: 106, align: "left", font: t.body, color: t.fg, text: s.tag.toUpperCase() }),
      ...chartBlock(t, s, 40, 60),
      ...(s.caption ? [txt({ y: 106, font: t.tag, color: t.muted, field: s.caption })] : []),
    ];
  }

  const items: Item[] = [];
  const cells = s.metrics ?? [];

  items.push(solid(5, 5, 74, 62, t.accent));
  items.push(solid(83, 5, 40, 62, t.plateColor));
  items.push(solid(5, 71, 56, 52, t.plateColor));
  items.push(solid(65, 71, 58, 52, t.plateColor));

  if (s.hero) {
    items.push(
      txt({
        x: 11,
        y: 12,
        w: 62,
        align: "left",
        font: isText(s.hero) ? t.big : 34,
        color: t.bg.from,
        field: s.hero,
      }),
    );
  }
  items.push(
    txt({
      x: 11,
      y: 50,
      w: 62,
      align: "left",
      font: t.tag,
      color: t.bg.from,
      text: (s.unit ?? s.tag).toUpperCase(),
    }),
  );

  if (cells[0]) {
    items.push(txt({ x: 87, y: 12, w: 32, align: "left", font: t.tag, color: t.muted, text: cells[0].label ?? "" }));
    items.push(txt({ x: 87, y: 24, w: 32, align: "left", font: t.body, color: tone(t, cells[0].tone ?? "fg"), field: cells[0] }));
  }
  if (cells[1]) {
    items.push(txt({ x: 87, y: 42, w: 32, align: "left", font: t.tag, color: t.muted, text: cells[1].label ?? "" }));
    items.push(txt({ x: 87, y: 52, w: 32, align: "left", font: t.body, color: tone(t, cells[1].tone ?? "fg"), field: cells[1] }));
  }

  if (s.body) {
    items.push(txt({ x: 11, y: 78, w: 106, align: "left", font: t.tag, color: t.fg, field: s.body, wrap: true, h: 40 }));
  } else {
    const a = cells[2] ?? cells[0];
    const b = cells[0] ?? cells[1];
    if (a) {
      items.push(txt({ x: 12, y: 78, w: 46, align: "left", font: t.tag, color: t.muted, text: a.label ?? "" }));
      items.push(txt({ x: 12, y: 92, w: 46, align: "left", font: t.big, color: tone(t, a.tone ?? "fg"), field: a }));
    }
    if (b && s.caption) {
      items.push(txt({ x: 72, y: 78, w: 46, align: "left", font: t.tag, color: t.muted, text: "AGORA" }));
      items.push(txt({ x: 72, y: 92, w: 46, align: "left", font: t.body, color: t.accent, field: s.caption }));
    } else if (b) {
      items.push(txt({ x: 72, y: 78, w: 46, align: "left", font: t.tag, color: t.muted, text: b.label ?? "" }));
      items.push(txt({ x: 72, y: 92, w: 46, align: "left", font: t.big, color: tone(t, b.tone ?? "fg"), field: b }));
    }
  }

  return items;
};

// 05 Swiss: grade rigida, tudo alinhado a esquerda, reguas grossas.
const swiss: Layout = (t, s) => {
  const items: Item[] = [
    tag(t, s, 8, M, INNER, "left"),
    rule(t, M, 18, INNER, 2, t.accent),
  ];

  if (s.rows) return [...items, ...rowsBlock(t, s, 26, false)];
  if (s.list) return [...items, ...listBlock(t, s, 26)];
  if (s.chart) {
    return [
      ...items,
      ...chartBlock(t, s, 28, 58),
      rule(t, M, 92, INNER),
      ...(s.caption
        ? [txt({ x: M, y: 100, align: "left", font: t.tag, color: t.muted, field: s.caption })]
        : []),
    ];
  }

  let y = 24;

  if (s.hero) {
    const font = isText(s.hero) ? 34 : 48;
    items.push(
      txt({ x: M, y, w: 104, align: "left", font, color: tone(t, s.hero.tone ?? "fg"), field: s.hero }),
    );
    if (s.unit) {
      items.push(txt({ x: 96, y: y + 12, w: 26, align: "left", font: t.tag, color: t.accent, text: s.unit }));
    }
    y += font + 8;
  }

  items.push(rule(t, M, y, INNER));
  y += 8;

  if (s.body) {
    items.push(txt({ x: M, y, w: INNER, align: "left", font: t.tag, color: t.fg, field: s.body, wrap: true, h: H - y - 6 }));
    return items;
  }

  if (s.caption) {
    items.push(txt({ x: M, y, w: INNER, align: "left", font: t.tag, color: tone(t, s.caption.tone ?? "fg"), field: s.caption, scroll: true }));
    y += t.tag + 6;
  }

  for (const cell of s.metrics ?? []) {
    items.push(txt({ x: M, y, w: 60, align: "left", font: t.tag, color: t.muted, text: cell.label ?? "" }));
    items.push(txt({ x: 68, y, w: 52, align: "right", font: t.tag, color: tone(t, cell.tone ?? "fg"), field: cell }));
    y += t.tag + 4;
  }

  return items;
};

// 16 Ficha tecnica: tabela densa de chave e valor.
const rowsLayout: Layout = (t, s) => {
  const items: Item[] = [
    txt({ x: 6, y: 6, w: 116, align: "left", font: t.tag, color: t.accent, text: s.tag.toUpperCase() }),
  ];

  if (s.chart) {
    return [...items, ...chartBlock(t, s, 24, 66), ...(s.caption ? [txt({ x: 6, y: 100, align: "left", font: t.tag, color: t.muted, field: s.caption })] : [])];
  }
  if (s.list) return [...items, ...listBlock(t, s, 22)];
  if (s.body) return [...items, ...bodyBlock(t, s, 22, 98)];

  const lines: { label: string; field: Field }[] = [];
  if (s.hero) lines.push({ label: (s.unit ?? s.title).toUpperCase(), field: s.hero });
  if (s.caption) lines.push({ label: "COND", field: s.caption });
  for (const cell of s.metrics ?? []) lines.push({ label: cell.label ?? "", field: cell });
  for (const row of s.rows ?? []) lines.push({ label: row.left.text ?? "", field: row.right });

  let y = 20;
  for (const line of lines.slice(0, 6)) {
    items.push(rule(t, 6, y + 15, 116));
    items.push(txt({ x: 8, y: y + 4, w: 56, align: "left", font: t.tag, color: t.muted, text: line.label }));
    items.push(
      txt({ x: 62, y: y + 2, w: 58, align: "right", font: t.body, color: tone(t, line.field.tone ?? "fg"), field: line.field }),
    );
    y += 18;
  }

  return items;
};

// 25 Split vertical: coluna solida a esquerda, metricas empilhadas a direita.
const split: Layout = (t, s) => {
  const items: Item[] = [solid(0, 0, 62, H, t.accent)];

  items.push(txt({ x: 8, y: 12, w: 48, align: "left", font: t.tag, color: t.bg.from, text: s.tag.toUpperCase() }));

  if (s.hero) {
    items.push(
      txt({ x: 8, y: 32, w: 50, align: "left", font: isText(s.hero) ? t.big : 34, color: t.bg.from, field: s.hero }),
    );
  }
  if (s.unit) items.push(txt({ x: 8, y: 74, w: 50, align: "left", font: t.tag, color: t.bg.from, text: s.unit }));
  if (s.caption) {
    items.push(txt({ x: 8, y: 92, w: 50, align: "left", font: t.tag, color: t.bg.from, field: s.caption, scroll: true }));
  }

  if (s.chart) {
    items.push({ t: "chart", x: 68, y: 20, w: 54, h: 88, color: tone(t, s.chart.tone), bind: s.chart.bind });
    return items;
  }

  if (s.body) {
    items.push(txt({ x: 68, y: 12, w: 54, align: "left", font: t.tag, color: t.fg, field: s.body, wrap: true, h: 104 }));
    return items;
  }

  const cells: { label: string; field: Field }[] = [
    ...(s.metrics ?? []).map((m) => ({ label: m.label ?? "", field: m })),
    ...(s.rows ?? []).map((r) => ({ label: r.left.text ?? "", field: r.right })),
    ...(s.list ?? []).map((l, i) => ({ label: `#${i + 1}`, field: l })),
  ];

  let y = 16;
  for (const cell of cells.slice(0, 3)) {
    items.push(txt({ x: 68, y, w: 54, align: "left", font: t.tag, color: t.muted, text: cell.label }));
    items.push(txt({ x: 68, y: y + 10, w: 54, align: "left", font: t.body, color: tone(t, cell.field.tone ?? "fg"), field: cell.field }));
    y += 34;
  }

  return items;
};

// 26 Barras de dados: cada metrica vira um trilho.
const bars: Layout = (t, s) => {
  const items: Item[] = [];

  if (s.hero) {
    items.push(txt({ x: M, y: 6, w: 60, align: "left", font: isText(s.hero) ? t.body : t.big, color: tone(t, s.hero.tone ?? "fg"), field: s.hero }));
  }
  items.push(txt({ x: 70, y: 12, w: 52, align: "right", font: t.tag, color: t.muted, text: s.tag.toUpperCase() }));

  if (s.chart) return [...items, ...chartBlock(t, s, 36, 74)];
  if (s.body) return [...items, ...bodyBlock(t, s, 34, 88)];
  if (s.list) return [...items, ...listBlock(t, s, 34)];

  const cells: { label: string; field: Field; max: number }[] = [
    ...(s.metrics ?? []).map((m) => ({ label: m.label ?? "", field: m, max: 100 })),
    ...(s.rows ?? []).map((r) => ({ label: r.left.text ?? "", field: r.right, max: 100 })),
  ];

  let y = 38;
  for (const cell of cells.slice(0, 3)) {
    items.push(txt({ x: M, y, w: 80, align: "left", font: t.tag, color: t.muted, text: cell.label }));
    items.push(txt({ x: 68, y, w: 52, align: "right", font: t.tag, color: tone(t, cell.field.tone ?? "fg"), field: cell.field }));
    items.push({
      t: "bar",
      x: M,
      y: y + 12,
      w: INNER,
      h: 8,
      color: tone(t, cell.field.tone ?? "accent"),
      track: t.line,
      ...(cell.field.bind ? { bind: cell.field.bind } : { value: 50 }),
      min: 0,
      max: cell.max,
    });
    y += 26;
  }

  if (s.caption && y < 108) {
    items.push(txt({ x: M, y, w: INNER, align: "left", font: t.tag, color: t.muted, field: s.caption, scroll: true }));
  }

  return items;
};

// 14 Arco de foco: anel grande como elemento principal.
const arcLayout: Layout = (t, s) => {
  if (s.rows) return [tag(t, s), ...rowsBlock(t, s, 24, t.plate !== "none")];
  if (s.list) return [tag(t, s), ...listBlock(t, s, 24)];
  if (s.chart) return [tag(t, s), ...chartBlock(t, s, 26, 66), ...(s.caption ? [txt({ y: 100, font: t.tag, color: t.muted, field: s.caption })] : [])];

  const items: Item[] = [
    {
      t: "arc",
      x: 14,
      y: 8,
      size: 100,
      width: 9,
      color: t.accent,
      track: t.line,
      ...(s.gauge
        ? { bind: s.gauge.bind, min: s.gauge.min, max: s.gauge.max }
        : { bind: "weather.hum", min: 0, max: 100 }),
    },
  ];

  if (s.moon) items.push({ t: "moon", x: 44, y: 38, size: 40, color: t.fg });

  if (s.hero) {
    items.push(
      txt({ x: 24, y: 40, w: 80, font: isText(s.hero) ? t.big : 34, color: tone(t, s.hero.tone ?? "fg"), field: s.hero }),
    );
  }
  if (s.unit) items.push(txt({ x: 24, y: 76, w: 80, font: t.tag, color: t.muted, text: s.unit }));

  if (s.body) {
    items.push(txt({ y: 96, font: t.tag, color: t.fg, field: s.body, wrap: true, h: 30 }));
    return items;
  }

  if (s.caption) {
    items.push(txt({ y: 100, font: t.tag, color: tone(t, s.caption.tone ?? "accent"), field: s.caption, scroll: true }));
  }

  const cells = s.metrics ?? [];
  if (cells.length && !s.caption) {
    const step = INNER / cells.length;
    cells.forEach((cell, i) => {
      items.push(txt({ x: M + step * i, y: 100, w: step, font: t.tag, color: t.muted, text: cell.label ?? "" }));
      items.push(txt({ x: M + step * i, y: 112, w: step, font: t.tag, color: tone(t, cell.tone ?? "fg"), field: cell }));
    });
  }

  return items;
};

// 18 Cartao empilhado: um card grande no topo, dois embaixo.
const cards: Layout = (t, s) => {
  if (s.list) return [tag(t, s, 8), ...listBlock(t, s, 24)];
  if (s.rows) return [tag(t, s, 8), ...rowsBlock(t, s, 24, true)];

  const items: Item[] = [
    solid(12, 5, 104, 18, t.plateColor),
    txt({ x: 16, y: 8, w: 96, font: t.tag, color: t.muted, text: s.tag.toUpperCase() }),
    solid(6, 27, 116, 52, t.accent),
  ];

  if (s.chart) {
    items.push({ t: "chart", x: 12, y: 33, w: 104, h: 40, color: t.bg.from, bind: s.chart.bind });
  } else if (s.hero) {
    items.push(
      txt({ x: 14, y: 34, w: 100, align: "left", font: isText(s.hero) ? t.big : 34, color: t.bg.from, field: s.hero }),
    );
    if (s.unit) {
      items.push(txt({ x: 14, y: 64, w: 100, align: "left", font: t.tag, color: t.bg.from, text: s.unit }));
    }
  } else if (s.moon) {
    items.push({ t: "moon", x: 48, y: 30, size: 44, color: t.bg.from });
  }

  if (s.body) {
    items.push(solid(6, 83, 116, 40, t.plateColor));
    items.push(txt({ x: 12, y: 88, w: 104, font: t.tag, color: t.fg, field: s.body, wrap: true, h: 30 }));
    return items;
  }

  const cells = s.metrics ?? [];
  const a = cells[0];
  const b = cells[1] ?? cells[2];

  if (a) {
    items.push(solid(6, 83, 55, 40, t.plateColor));
    items.push(txt({ x: 10, y: 88, w: 47, align: "left", font: t.tag, color: t.muted, text: a.label ?? "" }));
    items.push(txt({ x: 10, y: 100, w: 47, align: "left", font: t.big, color: tone(t, a.tone ?? "fg"), field: a }));
  }
  if (b) {
    items.push(solid(67, 83, 55, 40, t.plateColor));
    items.push(txt({ x: 71, y: 88, w: 47, align: "left", font: t.tag, color: t.muted, text: b.label ?? "" }));
    items.push(txt({ x: 71, y: 100, w: 47, align: "left", font: t.big, color: tone(t, b.tone ?? "fg"), field: b }));
  }
  if (!a && s.caption) {
    items.push(txt({ y: 92, font: t.body, color: tone(t, s.caption.tone ?? "fg"), field: s.caption, scroll: true }));
  }

  return items;
};

// 11 Numero gigante: o dado principal ocupa a tela.
const huge: Layout = (t, s) => {
  if (s.rows) return [tag(t, s, 6, M, INNER, "left"), ...rowsBlock(t, s, 22, false)];
  if (s.list) return [tag(t, s, 6, M, INNER, "left"), ...listBlock(t, s, 22)];
  if (s.chart) {
    return [
      tag(t, s, 6, M, INNER, "left"),
      ...chartBlock(t, s, 24, 70),
      ...(s.caption ? [txt({ x: M, y: 102, align: "left", font: t.tag, color: t.accent, field: s.caption })] : []),
    ];
  }
  if (s.body) {
    return [
      tag(t, s, 6, M, INNER, "left"),
      txt({ x: M, y: 22, w: INNER, align: "left", font: t.body, color: t.fg, field: s.body, wrap: true, h: 96 }),
    ];
  }

  const items: Item[] = [];

  if (s.hero) {
    const font = isText(s.hero) ? 40 : 48;
    items.push(txt({ x: 4, y: 4, w: 120, align: "left", font, color: tone(t, s.hero.tone ?? "accent"), field: s.hero }));
  }

  let y = 88;
  if (s.unit) {
    items.push(txt({ x: M, y, w: INNER, align: "left", font: t.tag, color: t.muted, text: s.unit.toUpperCase() }));
    y += 14;
  }
  if (s.caption) {
    items.push(txt({ x: M, y, w: INNER, align: "left", font: t.tag, color: t.muted, field: s.caption, scroll: true }));
    y += 14;
  }

  const cells = s.metrics ?? [];
  if (cells.length && y < 118) {
    const step = INNER / cells.length;
    cells.forEach((cell, i) => {
      items.push(
        txt({ x: M + step * i, y, w: step, align: "left", font: t.tag, color: tone(t, cell.tone ?? "accent"), field: cell }),
      );
    });
  }

  return items;
};

export const layouts: Record<string, Layout> = {
  hero,
  bento,
  swiss,
  rows: rowsLayout,
  split,
  bars,
  arc: arcLayout,
  cards,
  huge,
};

export type LayoutName = keyof typeof layouts;
