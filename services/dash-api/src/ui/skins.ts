import type { Skin } from "./skin.js";

export const skins: Record<string, Skin> = {
  glass: {
    tag: { x: 0, y: 8, w: 128, align: "center", font: 10, color: "#8b93a7", upper: true },
    hero: { x: 8, y: 26, w: 62, align: "left", font: 40, color: "#ffffff" },
    unit: { x: 70, y: 32, w: 30, align: "left", font: 14, color: "#f59e0b" },
    caption: { x: 0, y: 74, w: 128, align: "center", font: 10, color: "#9aa3b8" },
    metrics: [
      {
        box: { x: 8, y: 90, w: 112, h: 28, color: "#ffffff", style: "glass", r: 12 },
        label: { x: 10, y: 90, w: 36, align: "center", font: 10, color: "#8b93a7" },
        value: { x: 10, y: 99, w: 36, align: "center", font: 10, color: "#e8ecf4" },
      },
      {
        label: { x: 46, y: 90, w: 36, align: "center", font: 10, color: "#8b93a7" },
        value: { x: 46, y: 99, w: 36, align: "center", font: 10, color: "#f59e0b" },
      },
      {
        label: { x: 82, y: 90, w: 36, align: "center", font: 10, color: "#8b93a7" },
        value: { x: 82, y: 99, w: 36, align: "center", font: 10, color: "#e8ecf4" },
      },
    ],
    content: { x: 8, y: 40, w: 112, h: 44 },
  },

  bento: {
    orn: [
      { t: "plate", x: 5, y: 5, w: 74, h: 62, style: "solid", color: "#f59e0b", r: 9 },
      { t: "plate", x: 83, y: 5, w: 40, h: 62, style: "solid", color: "#1a2030", r: 9 },
      { t: "plate", x: 5, y: 71, w: 56, h: 52, style: "solid", color: "#1a2030", r: 9 },
      { t: "plate", x: 65, y: 71, w: 58, h: 52, style: "solid", color: "#1e293b", r: 9 },
    ],
    tag: { x: 11, y: 48, w: 66, align: "left", font: 10, color: "#1a1206", upper: true },
    hero: { x: 11, y: 12, w: 66, align: "left", font: 34, color: "#1a1206" },
    caption: { x: 89, y: 12, w: 34, align: "left", font: 10, color: "#7c88a1" },
    metrics: [
      {
        label: { x: 89, y: 12, w: 34, align: "left", font: 10, color: "#7c88a1", upper: true },
        value: { x: 89, y: 22, w: 34, align: "left", font: 16, color: "#ffffff" },
      },
      {
        label: { x: 12, y: 78, w: 46, align: "left", font: 10, color: "#7c88a1", upper: true },
        value: { x: 12, y: 90, w: 46, align: "left", font: 20, color: "#ffffff" },
      },
      {
        label: { x: 72, y: 78, w: 46, align: "left", font: 10, color: "#7c88a1", upper: true },
        value: { x: 72, y: 90, w: 46, align: "left", font: 20, color: "#f59e0b" },
      },
    ],
    content: { x: 8, y: 72, w: 112, h: 50 },
  },

  brutal: {
    orn: [
      { t: "plate", x: 8, y: 8, w: 112, h: 56, style: "solid", color: "#ffffff", r: 0 },
      { t: "plate", x: 8, y: 8, w: 112, h: 56, style: "outline", color: "#000000", r: 0, border: 3 },
      { t: "plate", x: 8, y: 100, w: 112, h: 20, style: "solid", color: "#000000", r: 0 },
    ],
    tag: { x: 16, y: 105, w: 104, align: "left", font: 10, color: "#fde047", upper: true },
    hero: { x: 14, y: 16, w: 100, align: "left", font: 40, color: "#000000" },
    caption: { x: 16, y: 105, w: 104, align: "left", font: 10, color: "#fde047", upper: true },
    metrics: [
      {
        box: { x: 8, y: 72, w: 52, h: 22, color: "#22d3ee", r: 0, border: 3 },
        label: { x: 12, y: 74, w: 44, align: "left", font: 10, color: "#000000", upper: true },
        value: { x: 12, y: 82, w: 44, align: "left", font: 10, color: "#000000" },
      },
      {
        box: { x: 68, y: 72, w: 52, h: 22, color: "#f472b6", r: 0, border: 3 },
        label: { x: 72, y: 74, w: 44, align: "left", font: 10, color: "#000000", upper: true },
        value: { x: 72, y: 82, w: 44, align: "left", font: 10, color: "#000000" },
      },
    ],
    content: { x: 12, y: 70, w: 104, h: 26 },
  },

  terminal: {
    orn: Array.from({ length: 42 }, (_, i) => ({
      t: "rule" as const,
      x: 0,
      y: i * 3,
      w: 128,
      h: 1,
      color: "#0b2b12",
    })),
    tag: { x: 6, y: 6, w: 116, align: "left", font: 10, color: "#4ade80" },
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
};
