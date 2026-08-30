import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, resolve } from "node:path";

import { buildPages } from "./pages.js";
import { themeById, themes } from "./themes.js";
import type { UiDoc } from "./types.js";

type Stored = { theme: string; version: number; hidden: string[] };

const FILE = resolve(process.env.DASH_DATA_DIR || "data", "ui.json");

const fallback: Stored = { theme: themes[0]!.id, version: 1, hidden: [] };

let state: Stored = load();

function load(): Stored {
  try {
    const raw = JSON.parse(readFileSync(FILE, "utf8")) as Partial<Stored>;
    return {
      theme: typeof raw.theme === "string" ? raw.theme : fallback.theme,
      version: typeof raw.version === "number" ? raw.version : fallback.version,
      hidden: Array.isArray(raw.hidden) ? raw.hidden.filter((id) => typeof id === "string") : [],
    };
  } catch {
    return { ...fallback };
  }
}

function persist(): void {
  try {
    mkdirSync(dirname(FILE), { recursive: true });
    writeFileSync(FILE, JSON.stringify(state, null, 2));
  } catch (error) {
    console.warn(`[ui] nao gravou ${FILE}: ${String(error)}`);
  }
}

export const uiVersion = (): { version: number; theme: string } => ({
  version: state.version,
  theme: state.theme,
});

export const uiDoc = (): UiDoc => {
  const theme = themeById(state.theme);
  return {
    version: state.version,
    theme: theme.id,
    themeName: theme.name,
    pages: buildPages(theme).filter((page) => !state.hidden.includes(page.id)),
  };
};

export const previewDoc = (themeId: string): UiDoc => {
  const theme = themeById(themeId);
  return {
    version: 0,
    theme: theme.id,
    themeName: theme.name,
    pages: buildPages(theme),
  };
};

export const setUi = (next: { theme?: string; hidden?: string[] }): UiDoc => {
  if (next.theme && themes.some((t) => t.id === next.theme)) state.theme = next.theme;
  if (Array.isArray(next.hidden)) state.hidden = next.hidden.filter((id) => typeof id === "string");

  state.version += 1;
  persist();
  return uiDoc();
};

export const themeList = () =>
  themes.map((t) => ({ id: t.id, name: t.name, desc: t.desc, active: t.id === state.theme }));

export const hiddenPages = (): string[] => [...state.hidden];
