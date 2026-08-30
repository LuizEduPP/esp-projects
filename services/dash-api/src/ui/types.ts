export type Blob = { x: number; y: number; r: number; color: string; opa: number };

export type Background = {
  kind: "solid" | "gradient" | "grid" | "dots";
  from: string;
  to?: string;
  line?: string;
  step?: number;
  blobs?: Blob[];
};

export type PlateStyle = "glass" | "solid" | "outline" | "none";

export type Align = "left" | "center" | "right";

export type LabelItem = {
  t: "label";
  x: number;
  y: number;
  w: number;
  font: number;
  color: string;
  align?: Align;
  text?: string;
  bind?: string;
  fmt?: string;
  wrap?: boolean;
  h?: number;
  scroll?: boolean;
};

export type PlateItem = {
  t: "plate";
  x: number;
  y: number;
  w: number;
  h: number;
  style: PlateStyle;
  color?: string;
  r?: number;
  opa?: number;
  border?: number;
};

export type RuleItem = { t: "rule"; x: number; y: number; w: number; h: number; color: string };

export type ArcItem = {
  t: "arc";
  x: number;
  y: number;
  size: number;
  width: number;
  color: string;
  track: string;
  bind?: string;
  value?: number;
  min?: number;
  max?: number;
};

export type BarItem = {
  t: "bar";
  x: number;
  y: number;
  w: number;
  h: number;
  color: string;
  track: string;
  bind?: string;
  value?: number;
  min?: number;
  max?: number;
};

export type ChartItem = {
  t: "chart";
  x: number;
  y: number;
  w: number;
  h: number;
  color: string;
  bind: string;
};

export type IconItem = {
  t: "icon";
  x: number;
  y: number;
  size: number;
  color: string;
  color2?: string;
  bind: string;
};

export type MoonItem = { t: "moon"; x: number; y: number; size: number; color: string };

export type NeedleItem = {
  t: "needle";
  x: number;
  y: number;
  size: number;
  color: string;
  track: string;
  bind: string;
};

export type Item =
  | LabelItem
  | PlateItem
  | RuleItem
  | ArcItem
  | BarItem
  | ChartItem
  | IconItem
  | MoonItem
  | NeedleItem;

export type Page = { id: string; title: string; bg: Background; items: Item[] };

export type UiDoc = {
  version: number;
  theme: string;
  themeName: string;
  pages: Page[];
};

export type Theme = {
  id: string;
  name: string;
  desc: string;
  layout: string;
  chrome: string;
  radius: number;
  border: number;
  bg: Background;
  fg: string;
  muted: string;
  accent: string;
  accent2: string;
  hot: string;
  cold: string;
  good: string;
  bad: string;
  line: string;
  plate: PlateStyle;
  plateColor: string;
  hero: number;
  big: number;
  body: number;
  tag: number;
  upper: boolean;
};
