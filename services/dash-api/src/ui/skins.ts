import type { Box, MetricSlot, Skin, Slot } from "./skin.js";
import type { Item } from "./types.js";

const cols = (
  y: number,
  labelColor: string,
  valueColor: string | string[],
  font = 10,
  labelFont = 10,
): MetricSlot[] =>
  [8, 48, 88].map((x, i) => ({
    label: { x, y, w: 34, align: "left" as const, font: labelFont, color: labelColor, upper: true },
    value: {
      x,
      y: y + 11,
      w: 34,
      align: "left" as const,
      font,
      color: Array.isArray(valueColor) ? valueColor[i]! : valueColor,
    },
  }));

const centerCols = (
  y: number,
  labelColor: string,
  valueColor: string | string[],
  font = 10,
): MetricSlot[] =>
  [0, 43, 86].map((x, i) => ({
    label: { x, y, w: 42, align: "center" as const, font: 10, color: labelColor, upper: true },
    value: {
      x,
      y: y + 11,
      w: 42,
      align: "center" as const,
      font,
      color: Array.isArray(valueColor) ? valueColor[i]! : valueColor,
    },
  }));

const rule = (x: number, y: number, w: number, h: number, color: string): Item => ({
  t: "rule",
  x,
  y,
  w,
  h,
  color,
});

const plate = (
  x: number,
  y: number,
  w: number,
  h: number,
  color: string,
  r = 0,
  style: Box["style"] = "solid",
  border = 1,
): Item => ({ t: "plate", x, y, w, h, style, color, r, border });

const card = (x: number, y: number, w: number, h: number, color: string, r: number): Item =>
  plate(x, y, w, h, color, r);

const boxed = (
  slots: { x: number; y: number; w: number; h: number; color: string; r?: number; border?: number }[],
  labelColor: string,
  valueColors: string[],
  valueFont = 16,
): MetricSlot[] =>
  slots.map((b, i) => ({
    box: { x: b.x, y: b.y, w: b.w, h: b.h, color: b.color, r: b.r ?? 0, border: b.border ?? 1 },
    label: {
      x: b.x + 6,
      y: b.y + 6,
      w: b.w - 10,
      align: "left" as const,
      font: 10,
      color: labelColor,
      upper: true,
    },
    value: {
      x: b.x + 6,
      y: b.y + 18,
      w: b.w - 10,
      align: "left" as const,
      font: valueFont,
      color: valueColors[i]!,
    },
  }));

export const skins: Record<string, Skin> = {
  glass: {
    tag: { x: 0, y: 8, w: 128, align: "center", font: 10, color: "#8b93a7", upper: true },
    hero: { x: 8, y: 26, w: 62, align: "left", font: 40, color: "#ffffff" },
    unit: { x: 70, y: 32, w: 30, align: "left", font: 14, color: "#f59e0b" },
    caption: { x: 0, y: 74, w: 128, align: "center", font: 10, color: "#9aa3b8" },
    metrics: [
      {
        box: { x: 8, y: 90, w: 112, h: 28, color: "#ffffff", style: "glass", r: 12 },
        label: { x: 10, y: 92, w: 36, align: "center", font: 10, color: "#8b93a7" },
        value: { x: 10, y: 102, w: 36, align: "center", font: 10, color: "#e8ecf4" },
      },
      {
        label: { x: 46, y: 92, w: 36, align: "center", font: 10, color: "#8b93a7" },
        value: { x: 46, y: 102, w: 36, align: "center", font: 10, color: "#f59e0b" },
      },
      {
        label: { x: 82, y: 92, w: 36, align: "center", font: 10, color: "#8b93a7" },
        value: { x: 82, y: 102, w: 36, align: "center", font: 10, color: "#e8ecf4" },
      },
    ],
    content: { x: 8, y: 40, w: 112, h: 44 },
  },

  bento: {
    orn: [
      card(5, 5, 74, 62, "#f59e0b", 9),
      card(83, 5, 40, 62, "#1a2030", 9),
      card(5, 71, 56, 52, "#1a2030", 9),
      card(65, 71, 58, 52, "#1e293b", 9),
    ],
    hero: { x: 11, y: 12, w: 66, align: "left", font: 34, color: "#1a1206" },
    caption: { x: 11, y: 50, w: 66, align: "left", font: 10, color: "#1a1206", upper: true },
    metrics: [
      {
        label: { x: 12, y: 78, w: 46, align: "left", font: 10, color: "#7c88a1", upper: true },
        value: { x: 12, y: 90, w: 46, align: "left", font: 20, color: "#ffffff" },
      },
      {
        label: { x: 89, y: 12, w: 34, align: "left", font: 10, color: "#7c88a1", upper: true },
        value: { x: 89, y: 24, w: 34, align: "left", font: 16, color: "#ffffff" },
      },
      {
        label: { x: 72, y: 78, w: 46, align: "left", font: 10, color: "#7c88a1", upper: true },
        value: { x: 72, y: 90, w: 46, align: "left", font: 20, color: "#f59e0b" },
      },
    ],
    textBox: { x: 9, y: 74, w: 110, h: 46 },
    content: { x: 8, y: 72, w: 112, h: 50 },
  },

  brutal: {
    orn: [
      plate(8, 8, 112, 56, "#ffffff", 0),
      plate(8, 8, 112, 56, "#000000", 0, "outline", 3),
      plate(8, 100, 112, 20, "#000000", 0),
    ],
    hero: { x: 14, y: 16, w: 100, align: "left", font: 40, color: "#000000" },
    caption: { x: 14, y: 104, w: 100, align: "left", font: 10, color: "#fde047", upper: true },
    metrics: [
      {
        box: { x: 8, y: 72, w: 52, h: 22, color: "#22d3ee", r: 0, border: 3 },
        label: { x: 12, y: 74, w: 44, align: "left", font: 10, color: "#000000", upper: true },
        value: { x: 12, y: 83, w: 44, align: "left", font: 10, color: "#000000" },
      },
      {
        box: { x: 68, y: 72, w: 52, h: 22, color: "#f472b6", r: 0, border: 3 },
        label: { x: 72, y: 74, w: 44, align: "left", font: 10, color: "#000000", upper: true },
        value: { x: 72, y: 83, w: 44, align: "left", font: 10, color: "#000000" },
      },
    ],
    textBox: { x: 14, y: 14, w: 100, h: 44 },
    content: { x: 12, y: 70, w: 104, h: 26 },
  },

  terminal: {
    orn: Array.from({ length: 42 }, (_, i) => rule(0, i * 3, 128, 1, "#0b2b12")),
    tag: { x: 6, y: 6, w: 116, align: "left", font: 10, color: "#4ade80", upper: true },
    hero: { x: 6, y: 22, w: 116, align: "left", font: 28, color: "#86efac" },
    caption: { x: 6, y: 58, w: 116, align: "left", font: 10, color: "#4ade80" },
    metrics: [
      {
        label: { x: 6, y: 72, w: 58, align: "left", font: 10, color: "#4ade80", upper: true },
        value: { x: 64, y: 72, w: 58, align: "right", font: 10, color: "#86efac" },
      },
      {
        label: { x: 6, y: 86, w: 58, align: "left", font: 10, color: "#4ade80", upper: true },
        value: { x: 64, y: 86, w: 58, align: "right", font: 10, color: "#86efac" },
      },
      {
        label: { x: 6, y: 100, w: 58, align: "left", font: 10, color: "#4ade80", upper: true },
        value: { x: 64, y: 100, w: 58, align: "right", font: 10, color: "#86efac" },
      },
    ],
    content: { x: 6, y: 56, w: 116, h: 58 },
  },

  swiss: {
    orn: [rule(8, 18, 112, 2, "#111111"), rule(8, 80, 112, 1, "#111111")],
    tag: { x: 8, y: 7, w: 112, align: "left", font: 10, color: "#111111", upper: true },
    hero: { x: 8, y: 20, w: 88, align: "left", font: 48, color: "#111111" },
    unit: { x: 96, y: 30, w: 26, align: "left", font: 10, color: "#dc2626" },
    caption: { x: 8, y: 86, w: 112, align: "left", font: 10, color: "#111111", upper: true },
    metrics: cols(100, "#111111", ["#111111", "#111111", "#dc2626"]),
    content: { x: 8, y: 24, w: 112, h: 52 },
  },

  neon: {
    orn: [plate(14, 74, 100, 3, "#e879f9", 2)],
    tag: { x: 0, y: 10, w: 128, align: "center", font: 10, color: "#67e8f9", upper: true },
    hero: { x: 0, y: 24, w: 128, align: "center", font: 40, color: "#f0abfc" },
    caption: { x: 0, y: 84, w: 128, align: "center", font: 10, color: "#67e8f9", upper: true },
    metrics: cols(100, "#67e8f9", ["#a5b4fc", "#a5b4fc", "#fda4af"]),
    content: { x: 8, y: 32, w: 112, h: 38 },
  },

  eink: {
    orn: [rule(10, 94, 108, 1, "#c9c6bd")],
    tag: { x: 10, y: 12, w: 108, align: "left", font: 10, color: "#5a5852" },
    hero: { x: 10, y: 26, w: 108, align: "left", font: 48, color: "#1c1c1a" },
    caption: { x: 10, y: 80, w: 108, align: "left", font: 10, color: "#5a5852" },
    metrics: cols(100, "#5a5852", "#1c1c1a"),
    content: { x: 10, y: 30, w: 108, h: 46 },
  },

  lcd: {
    orn: [
      plate(6, 76, 52, 20, "#1f2a17", 3, "outline", 2),
      plate(70, 76, 52, 20, "#1f2a17", 3, "outline", 2),
    ],
    tag: { x: 8, y: 8, w: 112, align: "left", font: 10, color: "#3c4a32", upper: true },
    hero: { x: 8, y: 24, w: 112, align: "left", font: 40, color: "#1f2a17" },
    caption: { x: 8, y: 62, w: 112, align: "left", font: 10, color: "#3c4a32", upper: true },
    pick: [1, 2],
    metrics: [
      {
        label: { x: 10, y: 81, w: 26, align: "left", font: 10, color: "#3c4a32", upper: true },
        value: { x: 34, y: 81, w: 22, align: "right", font: 10, color: "#1f2a17" },
      },
      {
        label: { x: 75, y: 81, w: 22, align: "left", font: 10, color: "#3c4a32", upper: true },
        value: { x: 96, y: 81, w: 24, align: "right", font: 10, color: "#1f2a17" },
      },
    ],
    textBox: { x: 8, y: 24, w: 112, h: 48 },
    content: { x: 8, y: 60, w: 112, h: 38 },
  },

  mesh: {
    tag: { x: 0, y: 12, w: 128, align: "center", font: 10, color: "#ffffff", upper: true },
    hero: { x: 0, y: 28, w: 128, align: "center", font: 48, color: "#ffffff" },
    caption: { x: 0, y: 82, w: 128, align: "center", font: 10, color: "#ffffff" },
    metrics: centerCols(98, "#ffffff", "#ffffff"),
    content: { x: 8, y: 40, w: 112, h: 38 },
  },

  dot: {
    tag: { x: 0, y: 12, w: 128, align: "center", font: 10, color: "#dc2626", upper: true },
    hero: { x: 0, y: 26, w: 128, align: "center", font: 40, color: "#ffffff" },
    caption: { x: 0, y: 78, w: 128, align: "center", font: 10, color: "#888888", upper: true },
    metrics: centerCols(96, "#888888", "#ffffff"),
    content: { x: 8, y: 40, w: 112, h: 34 },
  },

  huge: {
    hero: { x: 2, y: 2, w: 124, align: "left", font: 48, color: "#f59e0b" },
    caption: { x: 8, y: 92, w: 112, align: "left", font: 10, color: "#9ca3af", upper: true },
    metrics: cols(106, "#9ca3af", "#f59e0b"),
    content: { x: 8, y: 56, w: 112, h: 32 },
  },

  duotone: {
    orn: [plate(0, 0, 128, 46, "#f43f5e", 0)],
    hero: { x: 10, y: 6, w: 108, align: "left", font: 34, color: "#ffffff" },
    caption: { x: 12, y: 50, w: 106, align: "left", font: 10, color: "#ffe4e6", upper: true },
    metrics: [
      {
        label: { x: 10, y: 86, w: 36, align: "left", font: 10, color: "#94a3b8", upper: true },
        value: { x: 10, y: 96, w: 36, align: "left", font: 16, color: "#ffffff" },
      },
      {
        label: { x: 48, y: 86, w: 36, align: "left", font: 10, color: "#94a3b8", upper: true },
        value: { x: 48, y: 96, w: 36, align: "left", font: 16, color: "#ffffff" },
      },
      {
        label: { x: 86, y: 86, w: 36, align: "left", font: 10, color: "#94a3b8", upper: true },
        value: { x: 86, y: 96, w: 36, align: "left", font: 16, color: "#f43f5e" },
      },
    ],
    textBox: { x: 10, y: 52, w: 108, h: 62 },
    content: { x: 10, y: 54, w: 108, h: 28 },
  },

  braun: {
    orn: [plate(0, 0, 128, 62, "#2b2b28", 0), plate(110, 12, 6, 6, "#f97316", 3)],
    tag: { x: 10, y: 10, w: 96, align: "left", font: 10, color: "#9a9a92", upper: true },
    hero: { x: 10, y: 22, w: 96, align: "left", font: 34, color: "#f2f2ee" },
    caption: { x: 10, y: 82, w: 108, align: "left", font: 12, color: "#2b2b28" },
    metrics: cols(102, "#5f5f58", "#2b2b28"),
    content: { x: 10, y: 68, w: 108, h: 30 },
  },

  arc: {
    orn: [plate(14, 8, 100, 100, "#1e293b", 50, "outline", 8)],
    hero: { x: 24, y: 38, w: 80, align: "center", font: 34, color: "#ffffff" },
    caption: { x: 0, y: 112, w: 128, align: "center", font: 10, color: "#f59e0b", upper: true },
    metrics: [
      {
        label: { x: 24, y: 70, w: 26, align: "center", font: 10, color: "#94a3b8", upper: true },
        value: { x: 24, y: 80, w: 26, align: "center", font: 10, color: "#ffffff" },
      },
      {
        label: { x: 51, y: 70, w: 26, align: "center", font: 10, color: "#94a3b8", upper: true },
        value: { x: 51, y: 80, w: 26, align: "center", font: 10, color: "#f59e0b" },
      },
      {
        label: { x: 78, y: 70, w: 26, align: "center", font: 10, color: "#94a3b8", upper: true },
        value: { x: 78, y: 80, w: 26, align: "center", font: 10, color: "#ffffff" },
      },
    ],
    textBox: { x: 16, y: 28, w: 96, h: 62 },
    content: { x: 24, y: 26, w: 80, h: 76 },
  },

  aurora: {
    tag: { x: 0, y: 14, w: 128, align: "center", font: 10, color: "#a7f3d0", upper: true },
    hero: { x: 0, y: 30, w: 128, align: "center", font: 40, color: "#ecfeff" },
    caption: { x: 0, y: 84, w: 128, align: "center", font: 10, color: "#a7f3d0" },
    metrics: centerCols(100, "#a7f3d0", "#bae6fd"),
    content: { x: 8, y: 44, w: 112, h: 36 },
  },

  spec: {
    orn: [
      rule(6, 36, 116, 1, "#1f2937"),
      rule(6, 54, 116, 1, "#1f2937"),
      rule(6, 72, 116, 1, "#1f2937"),
      rule(6, 90, 116, 1, "#1f2937"),
    ],
    tag: { x: 6, y: 6, w: 116, align: "left", font: 10, color: "#f59e0b", upper: true },
    hero: { x: 62, y: 20, w: 60, align: "right", font: 20, color: "#f59e0b" },
    caption: { x: 8, y: 110, w: 114, align: "left", font: 10, color: "#4b5563", upper: true },
    metrics: [
      {
        label: { x: 8, y: 40, w: 60, align: "left", font: 10, color: "#6b7280", upper: true },
        value: { x: 62, y: 38, w: 60, align: "right", font: 12, color: "#e5e7eb" },
      },
      {
        label: { x: 8, y: 58, w: 60, align: "left", font: 10, color: "#6b7280", upper: true },
        value: { x: 62, y: 56, w: 60, align: "right", font: 12, color: "#e5e7eb" },
      },
      {
        label: { x: 8, y: 76, w: 60, align: "left", font: 10, color: "#6b7280", upper: true },
        value: { x: 62, y: 74, w: 60, align: "right", font: 12, color: "#e5e7eb" },
      },
    ],
    content: { x: 6, y: 22, w: 116, h: 84 },
  },

  pixel: {
    orn: [
      rule(8, 20, 112, 2, "#41a6f6"),
      plate(8, 90, 52, 16, "#38b764", 0),
      plate(68, 90, 52, 16, "#ef7d57", 0),
    ],
    tag: { x: 8, y: 7, w: 112, align: "left", font: 10, color: "#ef7d57", upper: true },
    hero: { x: 8, y: 26, w: 112, align: "left", font: 40, color: "#ffcd75" },
    caption: { x: 8, y: 74, w: 112, align: "left", font: 10, color: "#94b0c2", upper: true },
    metrics: [
      {
        label: { x: 8, y: 110, w: 30, align: "left", font: 10, color: "#94b0c2", upper: true },
        value: { x: 38, y: 110, w: 24, align: "left", font: 10, color: "#ffffff" },
      },
      {
        label: { x: 11, y: 93, w: 26, align: "left", font: 10, color: "#1a1c2c", upper: true },
        value: { x: 37, y: 93, w: 22, align: "left", font: 10, color: "#1a1c2c" },
      },
      {
        label: { x: 71, y: 93, w: 24, align: "left", font: 10, color: "#1a1c2c", upper: true },
        value: { x: 95, y: 93, w: 24, align: "left", font: 10, color: "#1a1c2c" },
      },
    ],
    textBox: { x: 8, y: 26, w: 112, h: 58 },
    content: { x: 8, y: 40, w: 112, h: 30 },
  },

  stack: {
    orn: [
      card(12, 6, 104, 20, "#1f2937", 10),
      card(6, 22, 116, 56, "#2563eb", 10),
      card(6, 84, 55, 34, "#1f2937", 10),
      card(67, 84, 55, 34, "#1f2937", 10),
    ],
    tag: { x: 22, y: 10, w: 84, align: "left", font: 10, color: "#9ca3af", upper: true },
    hero: { x: 16, y: 32, w: 96, align: "left", font: 28, color: "#ffffff" },
    caption: { x: 16, y: 62, w: 100, align: "left", font: 10, color: "#dbeafe", upper: true },
    pick: [1, 2],
    metrics: boxed(
      [
        { x: 6, y: 84, w: 55, h: 34, color: "#1f2937", r: 10 },
        { x: 67, y: 84, w: 55, h: 34, color: "#1f2937", r: 10 },
      ],
      "#6b7280",
      ["#ffffff", "#f59e0b"],
      16,
    ),
    textBox: { x: 14, y: 28, w: 100, h: 46 },
    content: { x: 6, y: 84, w: 116, h: 34 },
  },

  blueprint: {
    orn: [rule(8, 18, 112, 1, "#38bdf8"), rule(8, 74, 112, 1, "#38bdf8")],
    tag: { x: 8, y: 7, w: 112, align: "left", font: 10, color: "#38bdf8", upper: true },
    hero: { x: 8, y: 22, w: 78, align: "left", font: 40, color: "#7dd3fc" },
    unit: { x: 88, y: 38, w: 34, align: "left", font: 10, color: "#38bdf8" },
    caption: { x: 8, y: 80, w: 112, align: "left", font: 10, color: "#38bdf8", upper: true },
    metrics: cols(98, "#38bdf8", "#7dd3fc"),
    content: { x: 8, y: 26, w: 112, h: 44 },
  },

  vapor: {
    orn: [
      plate(34, 20, 60, 60, "#fde047", 30),
      ...[0, 1, 2, 3, 4].map((i) => rule(0, 88 + i * 8, 128, 1, "#22d3ee")),
      ...[0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map((i) => rule(i * 13, 88, 1, 40, "#22d3ee")),
    ],
    textOrn: [plate(46, 2, 36, 36, "#fde047", 18)],
    tag: { x: 0, y: 6, w: 128, align: "center", font: 10, color: "#fce7f3", upper: true },
    hero: { x: 0, y: 30, w: 128, align: "center", font: 34, color: "#ffffff" },
    caption: { x: 0, y: 92, w: 128, align: "center", font: 10, color: "#ffffff", upper: true },
    metrics: centerCols(104, "#fce7f3", "#a5f3fc"),
    textBox: { x: 6, y: 42, w: 116, h: 80 },
    content: { x: 8, y: 84, w: 112, h: 28 },
  },

  material: {
    orn: [card(6, 6, 116, 62, "#4a4458", 16)],
    tag: { x: 18, y: 12, w: 96, align: "left", font: 10, color: "#cac4d0", upper: true },
    hero: { x: 18, y: 24, w: 96, align: "left", font: 28, color: "#ffdfa0" },
    caption: { x: 18, y: 54, w: 98, align: "left", font: 10, color: "#e8def8" },
    pick: [1, 2],
    metrics: boxed(
      [
        { x: 6, y: 72, w: 56, h: 48, color: "#332d41", r: 16 },
        { x: 66, y: 72, w: 56, h: 48, color: "#332d41", r: 16 },
      ],
      "#cac4d0",
      ["#d0bcff", "#ffb4ab"],
      16,
    ),
    textBox: { x: 14, y: 14, w: 100, h: 48 },
    content: { x: 6, y: 72, w: 116, h: 48 },
  },

  oled: {
    tag: { x: 0, y: 10, w: 128, align: "center", font: 10, color: "#666666", upper: true },
    hero: { x: 0, y: 22, w: 128, align: "center", font: 48, color: "#ffffff" },
    caption: { x: 0, y: 82, w: 128, align: "center", font: 10, color: "#00e676", upper: true },
    metrics: centerCols(98, "#666666", "#ffffff", 12),
    content: { x: 8, y: 36, w: 112, h: 40 },
  },

  paper: {
    orn: [rule(8, 20, 112, 2, "#12100c"), rule(8, 94, 112, 1, "#12100c")],
    tag: { x: 8, y: 7, w: 112, align: "left", font: 10, color: "#12100c", upper: true },
    hero: { x: 8, y: 24, w: 112, align: "left", font: 40, color: "#12100c" },
    caption: { x: 8, y: 76, w: 112, align: "left", font: 12, color: "#12100c" },
    metrics: cols(100, "#12100c", "#12100c"),
    content: { x: 8, y: 28, w: 112, h: 60 },
  },

  neumorph: {
    orn: [
      card(8, 8, 112, 56, "#31343d", 14),
      card(8, 70, 52, 48, "#31343d", 14),
      card(68, 70, 52, 48, "#31343d", 14),
    ],
    tag: { x: 18, y: 14, w: 94, align: "left", font: 10, color: "#8b91a1", upper: true },
    hero: { x: 18, y: 26, w: 94, align: "left", font: 28, color: "#dfe3ec" },
    caption: { x: 18, y: 52, w: 96, align: "left", font: 10, color: "#8b91a1" },
    pick: [1, 2],
    metrics: boxed(
      [
        { x: 8, y: 70, w: 52, h: 48, color: "#31343d", r: 14 },
        { x: 68, y: 70, w: 52, h: 48, color: "#31343d", r: 14 },
      ],
      "#8b91a1",
      ["#7dd3fc", "#fbbf24"],
      16,
    ),
    textBox: { x: 14, y: 14, w: 100, h: 46 },
    content: { x: 8, y: 70, w: 114, h: 48 },
  },

  split: {
    orn: [plate(0, 0, 62, 128, "#0ea5e9", 0)],
    textOrn: [plate(0, 0, 128, 26, "#0ea5e9", 0)],
    tag: { x: 8, y: 14, w: 50, align: "left", font: 10, color: "#bae6fd", upper: true },
    hero: { x: 8, y: 32, w: 52, align: "left", font: 34, color: "#ffffff" },
    caption: { x: 8, y: 76, w: 52, align: "left", font: 10, color: "#e0f2fe", upper: true },
    metrics: [
      {
        label: { x: 70, y: 16, w: 52, align: "left", font: 10, color: "#64748b", upper: true },
        value: { x: 70, y: 26, w: 52, align: "left", font: 16, color: "#e2e8f0" },
      },
      {
        label: { x: 70, y: 50, w: 52, align: "left", font: 10, color: "#64748b", upper: true },
        value: { x: 70, y: 60, w: 52, align: "left", font: 16, color: "#e2e8f0" },
      },
      {
        label: { x: 70, y: 84, w: 52, align: "left", font: 10, color: "#64748b", upper: true },
        value: { x: 70, y: 94, w: 52, align: "left", font: 16, color: "#e2e8f0" },
      },
    ],
    textBox: { x: 6, y: 32, w: 116, h: 88 },
    content: { x: 68, y: 12, w: 54, h: 104 },
  },

  bars: {
    hero: { x: 8, y: 6, w: 64, align: "left", font: 24, color: "#ffffff" },
    caption: { x: 72, y: 14, w: 50, align: "left", font: 10, color: "#7d8794", upper: true },
    metrics: [
      {
        label: { x: 8, y: 44, w: 70, align: "left", font: 10, color: "#7d8794", upper: true },
        value: { x: 78, y: 42, w: 44, align: "right", font: 14, color: "#f59e0b" },
      },
      {
        label: { x: 8, y: 70, w: 70, align: "left", font: 10, color: "#7d8794", upper: true },
        value: { x: 78, y: 68, w: 44, align: "right", font: 14, color: "#38bdf8" },
      },
      {
        label: { x: 8, y: 96, w: 70, align: "left", font: 10, color: "#7d8794", upper: true },
        value: { x: 78, y: 94, w: 44, align: "right", font: 14, color: "#818cf8" },
      },
    ],
    content: { x: 8, y: 40, w: 112, h: 72 },
  },

  sticker: {
    orn: [
      plate(-14, -14, 70, 70, "#fb7185", 35),
      plate(80, 60, 70, 70, "#38bdf8", 35),
      plate(10, 26, 108, 52, "#ffffff", 16),
      plate(10, 26, 108, 52, "#111111", 16, "outline", 2),
    ],
    tag: { x: 22, y: 30, w: 90, align: "left", font: 10, color: "#111111", upper: true },
    hero: { x: 22, y: 40, w: 90, align: "left", font: 34, color: "#111111" },
    caption: { x: 12, y: 84, w: 106, align: "left", font: 10, color: "#111111", upper: true },
    metrics: cols(98, "#111111", "#111111"),
    textBox: { x: 16, y: 32, w: 96, h: 42 },
    content: { x: 12, y: 84, w: 106, h: 34 },
  },

  metric: {
    tag: { x: 0, y: 8, w: 128, align: "center", font: 10, color: "#64748b", upper: true },
    hero: { x: 0, y: 20, w: 128, align: "center", font: 40, color: "#e2e8f0" },
    caption: { x: 0, y: 68, w: 128, align: "center", font: 10, color: "#22d3ee" },
    metrics: centerCols(88, "#64748b", "#22d3ee", 14),
    content: { x: 8, y: 40, w: 112, h: 40 },
  },

  frame: {
    orn: [plate(5, 5, 118, 118, "#2b3446", 6, "outline", 1), plate(57, 32, 14, 2, "#f59e0b", 0)],
    tag: { x: 0, y: 18, w: 128, align: "center", font: 10, color: "#64748b", upper: true },
    hero: { x: 0, y: 38, w: 128, align: "center", font: 40, color: "#f8fafc" },
    caption: { x: 0, y: 86, w: 128, align: "center", font: 10, color: "#94a3b8", upper: true },
    metrics: centerCols(100, "#64748b", "#f8fafc"),
    content: { x: 12, y: 46, w: 104, h: 36 },
  },

  halfmoon: {
    orn: [plate(-20, -56, 168, 110, "#f97316", 55)],
    textOrn: [plate(-20, -78, 168, 110, "#f97316", 55)],
    tag: { x: 0, y: 16, w: 128, align: "center", font: 10, color: "#ffedd5", upper: true },
    hero: { x: 0, y: 40, w: 128, align: "center", font: 40, color: "#ffffff" },
    caption: { x: 0, y: 88, w: 128, align: "center", font: 10, color: "#fdba74", upper: true },
    metrics: centerCols(104, "#94a3b8", "#e2e8f0"),
    textBox: { x: 8, y: 40, w: 112, h: 80 },
    content: { x: 8, y: 58, w: 112, h: 28 },
  },
};
