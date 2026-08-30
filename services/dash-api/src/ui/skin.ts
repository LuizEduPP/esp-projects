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

export const applySkin = (t: Theme, skin: Skin, s: Screen): Item[] => {
  const isText = Boolean(s.body || s.list);
  const items: Item[] = [...((isText && skin.textOrn) || skin.orn || [])];

  if (skin.tag) {
    const tagValue = skin.tag.upper ? s.tag.toUpperCase() : s.tag;
    items.push(text(skin.tag, tagValue, s.tagBind));
  }

  if (s.hero) {
    items.push(fieldText(s.hero, skin.hero));
    if (s.unit && skin.unit) items.push(text(skin.unit, s.unit));
  }

  if (s.caption && skin.caption) items.push(fieldText(s.caption, skin.caption));

  const box = isText && skin.textBox ? skin.textBox : skin.content;

  if (s.chart) {
    items.push({
      t: "chart",
      x: box.x,
      y: box.y,
      w: box.w,
      h: box.h,
      bind: s.chart.bind,
      color: t[s.chart.tone === "fg" ? "fg" : s.chart.tone],
    });
  }

  if (s.gauge) {
    const size = Math.min(box.w, box.h);
    items.push({
      t: "arc",
      x: box.x + (box.w - size) / 2,
      y: box.y,
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
    items.push({
      t: "moon",
      x: box.x + (box.w - 48) / 2,
      y: box.y,
      size: 48,
      color: t.fg,
    });
  }

  if (s.needle) {
    items.push({
      t: "needle",
      x: box.x + box.w - 40,
      y: box.y,
      size: 38,
      bind: s.needle,
      color: t.accent,
      track: t.line,
    });
  }

  if (s.body) {
    items.push({
      t: "label",
      x: box.x,
      y: box.y,
      w: box.w,
      h: box.h,
      align: "left",
      font: t.body,
      color: t[s.body.tone ?? "fg"],
      wrap: true,
      bind: s.body.bind,
    });
  }

  if (s.list) {
    const step = Math.floor(box.h / s.list.length);
    s.list.forEach((f, i) => {
      items.push({
        t: "label",
        x: box.x,
        y: box.y + i * step,
        w: box.w,
        h: step - 2,
        align: "left",
        font: t.body,
        color: t[f.tone ?? "fg"],
        wrap: true,
        ...(f.bind ? { bind: f.bind } : { text: f.text ?? "" }),
      });
    });
  }

  if (s.rows) {
    const step = Math.floor(box.h / s.rows.length);
    s.rows.forEach((r, i) => {
      const y = box.y + i * step;
      items.push(
        fieldText(r.left, {
          x: box.x,
          y,
          w: Math.floor(box.w * 0.55),
          align: "left",
          font: t.tag,
          color: t[r.left.tone ?? "muted"],
          upper: true,
        }),
      );
      items.push(
        fieldText(r.right, {
          x: box.x + Math.floor(box.w * 0.55),
          y: y - 3,
          w: box.w - Math.floor(box.w * 0.55),
          align: "right",
          font: t.big,
          color: t[r.right.tone ?? "fg"],
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
      if (slot.box) items.push(boxItem(slot.box));
      items.push(text(slot.label, (m.label ?? "").toUpperCase()));
      items.push(fieldText(m, slot.value));
    }
  } else if (s.metrics && skin.strip) {
    const strip = joinStrip(s.metrics);
    if (strip) items.push(text(skin.strip, "--", strip.bind, strip.fmt));
  }

  return items;
};
