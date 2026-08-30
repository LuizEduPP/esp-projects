import { previewCss, renderJs } from "./renderjs.js";

export const editorPage = (): string => `<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>dash - painel</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background:#0b0d12; color:#e8ecf4; font:14px/1.5 system-ui,sans-serif; padding:28px 32px 88px; }
  h1 { font-size:20px; font-weight:650; }
  h2 { font-size:14px; font-weight:600; margin:34px 0 14px; color:#aab2c5; }
  .sub { color:#8b93a7; margin:6px 0 22px; max-width:74ch; }
  .grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(150px,1fr)); gap:20px 14px; }
  .theme { cursor:pointer; border:none; background:none; padding:0; text-align:left; color:inherit; }
  .frame { width:128px; height:128px; border-radius:8px; overflow:hidden;
    box-shadow:0 0 0 1px #232838; transition:box-shadow .12s; }
  .theme[aria-pressed=true] .frame { box-shadow:0 0 0 2px #38bdf8, 0 6px 18px #38bdf855; }
  .theme:hover .frame { box-shadow:0 0 0 2px #4b556b; }
  .cap { display:flex; gap:6px; align-items:baseline; margin-top:7px; }
  .num { color:#5c6478; font-size:11px; font-variant-numeric:tabular-nums; }
  .name { font-size:12px; font-weight:600; }
  .desc { font-size:11px; color:#7e879b; }
  .pages { display:flex; flex-wrap:wrap; gap:14px; }
  .page { display:flex; flex-direction:column; gap:6px; align-items:center; cursor:pointer; }
  .page .lbl { font-size:11px; color:#8b93a7; }
  .page.off .frame { opacity:.25; }
  .bar { position:fixed; left:0; right:0; bottom:0; padding:12px 32px; background:#11141ce6;
    border-top:1px solid #232838; display:flex; gap:14px; align-items:center; backdrop-filter:blur(8px); }
  .ok { color:#34d399; }
${previewCss}
</style>
</head>
<body>
<h1>dash</h1>
<p class="sub">Cada tema tem estrutura propria, nao so paleta: o layout muda onde o
valor principal, as metricas e as listas caem. A placa aplica a escolha em ate 10 segundos, sem
regravar nada. Clique numa tela da secao de baixo para tira-la do ciclo.</p>

<div class="grid" id="themes"></div>

<h2 id="pagesTitle">Telas do tema</h2>
<div class="pages" id="pages"></div>

<div class="bar"><span id="status">carregando...</span></div>

<script>
${renderJs}

let DATA = {};
let hidden = [];
let active = "";

async function loadData() {
  try {
    DATA = await (await fetch("/dash")).json();
  } catch (e) {
    status("sem dados do /dash - os valores aparecem como traco", false);
  }
}

async function loadThemes() {
  const info = await (await fetch("/ui/themes")).json();
  hidden = info.hidden;
  const box = document.getElementById("themes");
  box.textContent = "";

  let i = 0;
  for (const t of info.themes) {
    if (t.active) active = t.id;
    const preview = await (await fetch("/ui/preview/" + t.id)).json();
    const btn = document.createElement("button");
    btn.className = "theme";
    btn.setAttribute("aria-pressed", String(t.active));
    const frame = document.createElement("div");
    frame.className = "frame";
    frame.appendChild(renderPage(preview.pages[1] || preview.pages[0], DATA));
    btn.appendChild(frame);
    btn.insertAdjacentHTML("beforeend",
      '<div class="cap"><span class="num">' + String(++i).padStart(2, "0") +
      '</span><span class="name">' + t.name + '</span></div>' +
      '<div class="desc">' + t.desc + '</div>');
    btn.onclick = () => save({ theme: t.id });
    box.appendChild(btn);
  }
}

async function loadPages() {
  const doc = await (await fetch("/ui")).json();
  document.getElementById("pagesTitle").textContent = "Telas do tema " + doc.themeName;

  const box = document.getElementById("pages");
  box.textContent = "";

  const all = await (await fetch("/ui/preview/" + doc.theme)).json();
  for (const page of all.pages) {
    const off = hidden.indexOf(page.id) >= 0;
    const wrap = document.createElement("div");
    wrap.className = "page" + (off ? " off" : "");
    const frame = document.createElement("div");
    frame.className = "frame";
    frame.appendChild(renderPage(page, DATA));
    wrap.appendChild(frame);
    wrap.insertAdjacentHTML("beforeend",
      '<div class="lbl">' + page.title + (off ? " (oculta)" : "") + '</div>');
    wrap.onclick = () => {
      const next = off ? hidden.filter((id) => id !== page.id) : hidden.concat([page.id]);
      save({ hidden: next });
    };
    box.appendChild(wrap);
  }
}

async function save(body) {
  const res = await (await fetch("/ui", {
    method: "POST", headers: { "content-type": "application/json" }, body: JSON.stringify(body)
  })).json();
  status("salvo, versao " + res.version + " - a placa aplica em ate 10s", true);
  await refresh();
}

function status(text, ok) {
  const el = document.getElementById("status");
  el.textContent = text;
  el.className = ok ? "ok" : "";
}

async function refresh() { await loadThemes(); await loadPages(); }

(async () => {
  await loadData();
  await refresh();
  if (!document.getElementById("status").className) status("pronto", false);
})();
</script>
</body>
</html>
`;
