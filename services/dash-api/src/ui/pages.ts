import { screens } from "./content.js";
import { layouts } from "./layouts.js";
import type { Page, Theme } from "./types.js";

// Uma tela = conteudo abstrato (content.ts) passado pela familia do tema
// (layouts.ts). Trocar de tema troca a estrutura, nao so a paleta.
export const buildPages = (t: Theme): Page[] => {
  const layout = layouts[t.layout] ?? layouts.hero!;

  return screens.map((screen) => ({
    id: screen.id,
    title: screen.title,
    bg: t.bg,
    items: layout(t, screen),
  }));
};
