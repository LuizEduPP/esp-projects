import type { Item, Theme } from "./types.js";

const W = 128;
const H = 128;

type Chrome = (t: Theme) => Item[];

const none: Chrome = () => [];

const frame: Chrome = (t) => [
  { t: "plate", x: 5, y: 5, w: 118, h: 118, style: "outline", color: t.line, r: 6, border: 1 },
  { t: "rule", x: 57, y: 32, w: 14, h: 2, color: t.accent },
];

const scanlines: Chrome = (t) => {
  const items: Item[] = [];
  for (let y = 0; y < H; y += 4) {
    items.push({ t: "rule", x: 0, y, w: W, h: 1, color: t.line });
  }
  return items;
};

const halo: Chrome = (t) => [
  { t: "plate", x: 14, y: 74, w: 100, h: 3, style: "solid", color: t.accent, r: 2, opa: 200 },
  { t: "plate", x: 4, y: 66, w: 120, h: 20, style: "solid", color: t.accent, r: 10, opa: 26 },
];

const sunset: Chrome = (t) => [
  { t: "plate", x: 34, y: 18, w: 60, h: 60, style: "solid", color: "#fde047", r: 30, opa: 235 },
  { t: "plate", x: 34, y: 48, w: 60, h: 30, style: "solid", color: "#f97316", r: 15, opa: 180 },
  ...[0, 1, 2, 3, 4].map(
    (i): Item => ({ t: "rule", x: 0, y: 92 + i * 8, w: W, h: 1, color: t.accent2 }),
  ),
  ...[0, 1, 2, 3, 4, 5, 6, 7, 8, 9].map(
    (i): Item => ({ t: "rule", x: i * 14, y: 92, w: 1, h: 36, color: t.accent2 }),
  ),
];

const curve: Chrome = () => [];

const panel: Chrome = (t) => [
  { t: "plate", x: 0, y: 0, w: W, h: 62, style: "solid", color: "#2b2b28", r: 0 },
  { t: "plate", x: 108, y: 10, w: 8, h: 8, style: "solid", color: t.hot, r: 4 },
];

const diagonal: Chrome = (t) => {
  const items: Item[] = [];
  const steps = 16;
  for (let i = 0; i < steps; i++) {
    const x = (W / steps) * i;
    const h = 46 + Math.round((32 * (steps - i)) / steps);
    items.push({ t: "plate", x, y: 0, w: W / steps + 1, h, style: "solid", color: t.accent, r: 0 });
  }
  return items;
};

const blobs: Chrome = (t) => [
  { t: "plate", x: -14, y: -14, w: 70, h: 70, style: "solid", color: t.hot, r: 35 },
  { t: "plate", x: 76, y: 72, w: 70, h: 70, style: "solid", color: t.cold, r: 35 },
];

const hardEdge: Chrome = (t) => [
  { t: "plate", x: 0, y: 108, w: W, h: 20, style: "solid", color: "#000000", r: 0 },
  { t: "rule", x: 0, y: 104, w: W, h: 3, color: t.line },
];

const emboss: Chrome = (t) => [
  { t: "plate", x: 4, y: 4, w: 120, h: 120, style: "outline", color: t.line, r: 16, border: 2 },
];

const column: Chrome = (t) => [
  { t: "plate", x: 0, y: 0, w: 62, h: H, style: "solid", color: t.accent, r: 0 },
];

const grid: Chrome = (t) => {
  const items: Item[] = [];
  for (let y = 8; y < H; y += 8) items.push({ t: "rule", x: 0, y, w: W, h: 1, color: t.line });
  for (let x = 8; x < W; x += 8) items.push({ t: "rule", x, y: 0, w: 1, h: H, color: t.line });
  return items;
};

const bezel: Chrome = (t) => [
  { t: "plate", x: 3, y: 3, w: 122, h: 122, style: "outline", color: t.line, r: 3, border: 2 },
];

const masthead: Chrome = (t) => [
  { t: "rule", x: 8, y: 19, w: 112, h: 2, color: t.line },
  { t: "rule", x: 8, y: 23, w: 112, h: 1, color: t.line },
];

export const chromes: Record<string, Chrome> = {
  none,
  frame,
  scanlines,
  halo,
  sunset,
  curve,
  panel,
  diagonal,
  blobs,
  hardEdge,
  emboss,
  column,
  grid,
  bezel,
  masthead,
};

export const chromeOf = (t: Theme): Item[] => (chromes[t.chrome] ?? none)(t);
