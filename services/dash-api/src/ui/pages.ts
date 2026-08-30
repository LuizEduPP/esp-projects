import { chromeOf } from "./chrome.js";
import { screens } from "./content.js";
import { layouts } from "./layouts.js";
import { applySkin } from "./skin.js";
import { skins } from "./skins.js";
import type { Page, Theme } from "./types.js";

export const buildPages = (t: Theme): Page[] => {
  const skin = skins[t.id];
  const layout = layouts[t.layout] ?? layouts.hero!;
  const chrome = skin ? [] : chromeOf(t);

  return screens.map((screen) => ({
    id: screen.id,
    title: screen.title,
    bg: t.bg,
    items: skin ? applySkin(t, skin, screen) : [...chrome, ...layout(t, screen)],
  }));
};
