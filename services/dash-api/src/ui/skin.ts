import type { Field, Screen } from "./content.js";
import type { Align, Item, PlateStyle, Theme } from "./types.js";

export type Slot = {
  x: number;
  y: number;
  w?: number;
  h?: number;
  align?: Align;
  font: number;
  color: string;
  upper?: boolean;
  wrap?: boolean;
};

export type Box = {
  x: number;
  y: number;
  w: number;
  h: number;
  color: string;
  style?: PlateStyle;
  r?: number;
  border?: number;
};

export type MetricSlot = { box?: Box; label: Slot; value: Slot };

export type Skin = {
  orn?: Item[];
  textOrn?: Item[];
  tag?: Slot;
  hero: Slot;
  unit?: Slot;
  caption?: Slot;
  metrics?: MetricSlot[];
  pick?: number[];
  strip?: Slot;
  textBox?: { x: number; y: number; w: number; h: number };
  content: { x: number; y: number; w: number; h: number };
};

const text = (s: Slot, value: string, bind?: string, fmt?: string): Item => ({
  t: "label",
  x: s.x,
  y: s.y,
  w: s.w ?? 128 - s.x,
  ...(s.h === undefined ? {} : { h: s.h }),
  align: s.align ?? "left",
  font: s.font,
  color: s.color,
  ...(s.wrap ? { wrap: true } : {}),
  ...(s.upper ? { up: true } : {}),
  ...(bind ? { bind, ...(fmt ? { fmt } : {}) } : { text: value }),
});

const boxItem = (b: Box): Item => ({
  t: "plate",
  x: b.x,
  y: b.y,
  w: b.w,
  h: b.h,
  style: b.style ?? "solid",
  color: b.color,
  r: b.r ?? 0,
  border: b.border ?? 1,
});

const fieldText = (f: Field, s: Slot): Item => {
  const value = f.text === undefined ? "--" : s.upper ? f.text.toUpperCase() : f.text;
  return text(s, value, f.bind, f.fmt);
};

const joinStrip = (metrics: Field[]): { bind: string; fmt: string } | null => {
  const first = metrics.find((m) => m.bind);
  if (!first?.bind) return null;
  return { bind: first.bind, fmt: first.fmt ?? "%s" };
};

type Area = { x: number; y: number; w: number; h: number };

const textArea = (t: Theme, skin: Skin, box: Area): Area => {
  const line = Math.round(t.body * 1.15) + 2;
  const tagBottom = skin.tag ? skin.tag.y + Math.round(skin.tag.font * 1.35) : 2;
  const y = Math.max(box.y, tagBottom + 4);
  const x = Math.max(4, box.x);
  const w = Math.min(124, box.x + box.w) - x;
  const rows = Math.max(1, Math.floor((124 - y) / line));

  return { x, y, w, h: rows * line };
};

const clearsText = (it: Item, area: Area): boolean => {
  if (it.t !== "plate") return true;
  const inside = it.x >= 0 && it.y >= 0 && it.x + it.w <= 128 && it.y + it.h <= 128;
  const overlaps =
    it.x < area.x + area.w &&
    it.x + it.w > area.x &&
    it.y < area.y + area.h &&
    it.y + it.h > area.y;

  return !(inside && overlaps);
};

const FONTS = [8, 10, 12, 14, 16, 20, 24, 28, 34, 40, 48];

const fitFont = (max: number): number => {
  let best = FONTS[0]!;
  for (const f of FONTS) {
    if (f <= max) best = f;
  }
  return best;
};

const fitLabel = (font: number, chars: number, width: number): number =>
  chars * font * 0.68 <= width ? font : fitFont(Math.floor(width / (chars * 0.68)));

export const applySkin = (t: Theme, skin: Skin, s: Screen): Item[] => {
  const isText = Boolean(s.body || s.list);
  const ornaments = (isText && skin.textOrn) || skin.orn || [];
  const items: Item[] = isText
    ? ornaments.filter((it) => clearsText(it, textArea(t, skin, skin.textBox ?? skin.content)))
    : [...ornaments];

  if (skin.tag) {
    const tagValue = skin.tag.upper ? s.tag.toUpperCase() : s.tag;
    items.push(text(skin.tag, tagValue, s.tagBind));
  }

  if (s.hero) {
    items.push(fieldText(s.hero, skin.hero));
    if (s.unit && skin.unit) items.push(text(skin.unit, s.unit));
  }

  if (s.caption && skin.caption) items.push(fieldText(s.caption, skin.caption));

  const content = skin.content;
  const heroTop = Math.min(skin.hero.y, content.y);
  const freed = !s.hero && heroTop < content.y;
  const crowded = Boolean(s.metrics || s.caption);
  const heroRight = skin.hero.x + (skin.hero.w ?? 128 - skin.hero.x);
  const wraps = (it: Item) =>
    it.t === "plate" &&
    it.style !== "none" &&
    it.x >= 0 &&
    it.y >= 0 &&
    it.x + it.w <= 128 &&
    it.y + it.h <= 128 &&
    it.x <= skin.hero.x &&
    it.y <= skin.hero.y &&
    it.x + it.w >= heroRight &&
    it.y + it.h >= skin.hero.y;

  const card = (skin.orn ?? []).find(wraps);
  const floor = card && card.t === "plate" ? card.y + card.h - 4 : 128;
  const tall = Boolean(s.rows) && freed;

  if (tall) {
    for (let i = items.length - 1; i >= 0; i--) {
      if (wraps(items[i]!)) items.splice(i, 1);
    }
  }

  const nook =
    card && card.t === "plate" && s.hero
      ? (() => {
          const top = Math.max(
            card.y + 4,
            skin.tag ? skin.tag.y + Math.round(skin.tag.font * 1.35) : 0,
          );
          const size = Math.max(20, Math.min(40, card.y + card.h - 4 - top));
          return { x: card.x + card.w - size - 6, y: top, size };
        })()
      : null;

  const openTo = crowded ? content.y - 2 : content.y + content.h;
  const graph = !freed
    ? content
    : { x: content.x, y: heroTop, w: content.w, h: (tall ? openTo : Math.min(openTo, floor)) - heroTop };

  const box = isText && skin.textBox ? skin.textBox : freed ? graph : content;

  if (s.chart) {
    items.push({
      t: "chart",
      x: graph.x,
      y: graph.y,
      w: graph.w,
      h: graph.h,
      bind: s.chart.bind,
      color: freed && card ? skin.hero.color : t[s.chart.tone === "fg" ? "fg" : s.chart.tone],
    });
  }

  if (s.gauge) {
    const size = nook ? nook.size : Math.min(graph.w, graph.h);
    items.push({
      t: "arc",
      x: nook ? nook.x : graph.x + (graph.w - size) / 2,
      y: nook ? nook.y : graph.y,
      size,
      width: 8,
      min: s.gauge.min,
      max: s.gauge.max,
      bind: s.gauge.bind,
      color: t.accent,
      track: t.line,
    });
  }

  if (s.moon) {
    const size = nook ? nook.size : Math.min(48, graph.h - 4);
    items.push({
      t: "moon",
      x: nook ? nook.x : graph.x + (graph.w - size) / 2,
      y: nook ? nook.y : graph.y + Math.max(0, (graph.h - size) / 2),
      size,
      color: t.fg,
    });
  }

  if (s.needle) {
    items.push({
      t: "needle",
      x: nook ? nook.x : graph.x + graph.w - 40,
      y: nook ? nook.y : graph.y,
      size: nook ? nook.size - 2 : 38,
      bind: s.needle,
      color: t.accent,
      track: t.line,
    });
  }

  const flow: Field[] | undefined = s.body ? [s.body] : s.list;
  if (flow) {
    const area = textArea(t, skin, box);
    items.push({
      t: "stack",
      x: area.x,
      y: area.y,
      w: area.w,
      h: area.h,
      align: "left",
      lines: flow.map((f) => ({
        font: t.body,
        color: t[f.tone ?? "fg"],
        gap: 6,
        ...(f.bind ? { bind: f.bind, ...(f.fmt ? { fmt: f.fmt } : {}) } : { text: f.text ?? "" }),
      })),
    });
  }

  if (s.rows) {
    const area = freed ? graph : content;
    const step = Math.floor(area.h / s.rows.length);
    const valueFont = fitFont(Math.min(t.big, step - 2));
    const labelFont = fitFont(Math.min(t.tag, step - 2));
    const labelColor = skin.metrics?.[0]?.label.color ?? t[s.rows[0]!.left.tone ?? "muted"];
    const valueColor = skin.metrics?.[0]?.value.color ?? t[s.rows[0]!.right.tone ?? "fg"];
    const split = Math.floor(area.w * 0.55);

    s.rows.forEach((r, i) => {
      const y = area.y + i * step;
      items.push(
        fieldText(r.left, {
          x: area.x,
          y: y + Math.max(0, valueFont - labelFont) / 2,
          w: split,
          align: "left",
          font: labelFont,
          color: labelColor,
          upper: true,
        }),
      );
      items.push(
        fieldText(r.right, {
          x: area.x + split,
          y,
          w: area.w - split,
          align: "right",
          font: valueFont,
          color: valueColor,
        }),
      );
    });
  }

  if (s.metrics && skin.metrics) {
    const chosen = skin.pick
      ? skin.pick.map((i) => s.metrics![i]).filter((m): m is Field => m !== undefined)
      : s.metrics;
    const n = Math.min(chosen.length, skin.metrics.length);
    for (let i = 0; i < n; i++) {
      const m = chosen[i]!;
      const slot = skin.metrics[i]!;
      const caption = (m.label ?? "").toUpperCase();
      if (slot.box) items.push(boxItem(slot.box));
      items.push(
        text(
          { ...slot.label, font: fitLabel(slot.label.font, caption.length, slot.label.w ?? 128) },
          caption,
        ),
      );
      items.push(fieldText(m, slot.value));
    }
  } else if (s.metrics && skin.strip) {
    const strip = joinStrip(s.metrics);
    if (strip) items.push(text(skin.strip, "--", strip.bind, strip.fmt));
  }

  return items;
};
