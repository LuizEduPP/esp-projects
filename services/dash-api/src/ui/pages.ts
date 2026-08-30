import { screens } from "./content.js";
import { applySkin } from "./skin.js";
import { skins } from "./skins.js";
import type { Page, Theme } from "./types.js";

export const buildPages = (t: Theme): Page[] => {
  const skin = skins[t.id]!;

  return screens.map((screen) => ({
    id: screen.id,
    title: screen.title,
    bg: t.bg,
    items: applySkin(t, skin, screen),
  }));
};
