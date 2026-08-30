import { writeFileSync } from "node:fs";

import { buildPages } from "./pages.js";
import { previewCss, renderJs } from "./renderjs.js";
import { themes } from "./themes.js";

const source = process.env.DASH_API_URL ?? "http://127.0.0.1:8090";

const live = await fetch(`${source}/dash`)
  .then((res) => (res.ok ? res.json() : Promise.reject(new Error(`HTTP ${res.status}`))))
  .catch((error: Error) => {
    console.warn(`sem /dash em ${source} (${error.message}): as telas saem com traco`);
    return {};
  });

const docs = themes.map((theme) => ({
  id: theme.id,
  name: theme.name,
  desc: theme.desc,
  pages: buildPages(theme),
}));

const html = `<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<title>dash - 30 temas</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background:#0b0d12; color:#e8ecf4; font:14px/1.5 system-ui,sans-serif; padding:28px 34px 60px; }
  h1 { font-size:20px; font-weight:650; }
  .sub { color:#8b93a7; margin:6px 0 24px; max-width:76ch; }
  .theme { margin:0 0 30px; padding:0 0 24px; border-bottom:1px solid #1b1f2a; }
  .head { display:flex; gap:8px; align-items:baseline; margin-bottom:10px; }
  .num { color:#5c6478; font-size:12px; font-variant-numeric:tabular-nums; }
  .name { font-size:14px; font-weight:650; }
  .desc { color:#7e879b; font-size:12px; }
  .fam { margin-left:auto; color:#4b5568; font-size:11px; }
  .row { display:flex; flex-wrap:wrap; gap:12px; }
  .cell { display:flex; flex-direction:column; gap:5px; align-items:center; }
  .cell .lbl { font-size:10px; color:#6b7488; }
  .frame { width:128px; height:128px; border-radius:7px; overflow:hidden; box-shadow:0 0 0 1px #232838; }
${previewCss}
</style>
</head>
<body>
<h1>dash &middot; 30 temas &times; 19 telas</h1>
<p class="sub">Gerado pelos mesmos documentos que o servidor entrega a placa, com os dados reais
de ${source}/dash. Cada tema tem layout proprio, transcrito do mock: muda a
estrutura, nao so as cores. Campos que a placa gera sozinha (relogio, rede, timer) aparecem
como traco aqui.</p>
<div id="out"></div>
<script>
${renderJs}
const DATA = ${JSON.stringify(live)};
const DOCS = ${JSON.stringify(docs)};

const out = document.getElementById("out");
DOCS.forEach((doc, i) => {
  const box = document.createElement("div");
  box.className = "theme";
  box.innerHTML = '<div class="head"><span class="num">' + String(i + 1).padStart(2, "0") +
    '</span><span class="name">' + doc.name + '</span><span class="desc">' + doc.desc +
    '</span></div>';
  const row = document.createElement("div");
  row.className = "row";
  for (const page of doc.pages) {
    const cell = document.createElement("div");
    cell.className = "cell";
    const frame = document.createElement("div");
    frame.className = "frame";
    frame.appendChild(renderPage(page, DATA));
    cell.appendChild(frame);
    cell.insertAdjacentHTML("beforeend", '<div class="lbl">' + page.title + '</div>');
    row.appendChild(cell);
  }
  box.appendChild(row);
  out.appendChild(box);
});
</script>
</body>
</html>
`;

const target = process.argv[2] ?? "../../boards/c3/dash/preview/themes.html";
writeFileSync(target, html);
console.log(`gerado ${target} com ${docs.length} temas x ${docs[0]!.pages.length} telas`);
