export const editorPage = (): string => `<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>dash - painel</title>
<style>
  :root { color-scheme: dark; }
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { background:#0b0d12; color:#e8ecf4; font:14px/1.5 system-ui,sans-serif; padding:28px 32px 80px; }
  h1 { font-size:20px; font-weight:650; }
  h2 { font-size:14px; font-weight:600; margin:32px 0 12px; color:#aab2c5; }
  .sub { color:#8b93a7; margin:6px 0 22px; }
  .grid { display:grid; grid-template-columns:repeat(auto-fill,minmax(148px,1fr)); gap:18px 14px; }
  .theme { cursor:pointer; border:none; background:none; padding:0; text-align:left; color:inherit; }
  .frame { width:128px; height:128px; border-radius:8px; overflow:hidden; box-shadow:0 0 0 1px #232838; transition:box-shadow .12s; }
  .theme[aria-pressed=true] .frame { box-shadow:0 0 0 2px #38bdf8, 0 6px 18px #38bdf855; }
  .theme:hover .frame { box-shadow:0 0 0 2px #4b5costa; }
  .cap { margin-top:7px; font-size:12px; font-weight:600; }
  .desc { font-size:11px; color:#7e879b; }
  .screen { position:relative; width:128px; height:128px; overflow:hidden;
    font-family:"Montserrat","Segoe UI",system-ui,sans-serif; }
  .screen * { position:absolute; }
  .pages { display:flex; flex-wrap:wrap; gap:12px; }
  .page { display:flex; flex-direction:column; gap:6px; align-items:center; }
  .page .lbl { font-size:11px; color:#8b93a7; }
  .page.off .frame { opacity:.28; }
  .bar { position:fixed; left:0; right:0; bottom:0; padding:12px 32px; background:#11141ce6;
    border-top:1px solid #232838; display:flex; gap:14px; align-items:center; backdrop-filter:blur(8px); }
  .ok { color:#34d399; }
</style>
</head>
<body>
<h1>dash</h1>
<p class="sub">Escolha o tema. A placa aplica sozinha em ate 10 segundos, sem regravar nada.
Clique numa tela da secao de baixo para esconde-la do ciclo.</p>

<div class="grid" id="themes"></div>

<h2 id="pagesTitle">Telas do tema</h2>
<div class="pages" id="pages"></div>

<div class="bar"><span id="status">carregando...</span></div>

<script>
const SAMPLE = {
  time: "14:32", weekday: "DOM", date: "30 de agosto",
  sun: { progress: 62, daylight: "11h 43min" },
  timer: { clock: "25:00", mode: "foco", percent: 0 },
  net: { ssid: "hotspot", ip: "10.31.254.103", rssi: -58 },
  sys: { heap: 187, uptime: "2h 14m" }
};

let DATA = SAMPLE;
let doc = null;
let hidden = [];

const px = (n) => n + "px";
const get = (obj, path) => path.split(".").reduce((acc, k) => (acc == null ? acc : acc[k]), obj);

function fmt(spec, value) {
  if (value === undefined || value === null) return "--";
  if (!spec) return String(value);
  return spec.replace(/%[-0-9.]*[sdf]/, (m) => {
    if (m.endsWith("s")) return String(value);
    if (m.endsWith("d")) return String(Math.round(Number(value)));
    const dec = /\\.(\\d)/.exec(m);
    return Number(value).toFixed(dec ? Number(dec[1]) : 0);
  }).replace(/%%/g, "%");
}

function bgStyle(bg) {
  if (bg.kind === "gradient") return \`background:linear-gradient(160deg,\${bg.from},\${bg.to})\`;
  if (bg.kind === "grid")
    return \`background:\${bg.from};background-image:linear-gradient(\${bg.line} 1px,transparent 1px),\` +
      \`linear-gradient(90deg,\${bg.line} 1px,transparent 1px);background-size:\${bg.step}px \${bg.step}px\`;
  if (bg.kind === "dots")
    return \`background:\${bg.from};background-image:radial-gradient(\${bg.line} 1px,transparent 1px);\` +
      \`background-size:\${bg.step}px \${bg.step}px\`;
  return \`background:\${bg.from}\`;
}

function renderPage(page) {
  const el = document.createElement("div");
  el.className = "screen";
  el.setAttribute("style", bgStyle(page.bg));

  for (const b of page.bg.blobs || []) {
    const d = document.createElement("div");
    d.setAttribute("style", \`left:\${px(b.x - b.r)};top:\${px(b.y - b.r)};width:\${px(b.r * 2)};\` +
      \`height:\${px(b.r * 2)};border-radius:50%;background:\${b.color};opacity:\${b.opa / 255};filter:blur(14px)\`);
    el.appendChild(d);
  }

  for (const it of page.items) el.appendChild(renderItem(it));
  return el;
}

function renderItem(it) {
  const d = document.createElement("div");
  const at = (s) => d.setAttribute("style", s);

  if (it.t === "label") {
    const raw = it.bind ? get(DATA, it.bind) : it.text;
    d.textContent = it.text !== undefined && !it.bind ? it.text : fmt(it.fmt, raw);
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.w)};font-size:\${px(it.font)};\` +
      \`color:\${it.color};text-align:\${it.align || "center"};line-height:1.15;\` +
      \`overflow:hidden;\${it.h ? "height:" + px(it.h) + ";" : ""}\` +
      \`\${it.wrap ? "" : "white-space:nowrap;text-overflow:ellipsis;"}\`);
    return d;
  }

  if (it.t === "plate") {
    const style = it.style === "glass"
      ? \`background:\${it.color}18;border:1px solid \${it.color}30\`
      : it.style === "solid" ? \`background:\${it.color}\`
      : it.style === "outline" ? \`border:1px solid \${it.color}\` : "";
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.w)};height:\${px(it.h)};border-radius:10px;\${style}\`);
    return d;
  }

  if (it.t === "rule") {
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.w)};height:\${px(it.h)};background:\${it.color}\`);
    return d;
  }

  if (it.t === "arc" || it.t === "needle") {
    const v = it.bind ? Number(get(DATA, it.bind)) || 0 : (it.value || 0);
    const min = it.min ?? 0, max = it.max ?? 100;
    const pct = it.t === "needle" ? (v % 360) / 360 : Math.min(Math.max((v - min) / (max - min), 0), 1);
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.size)};height:\${px(it.size)};border-radius:50%;\` +
      \`background:conic-gradient(\${it.color} 0 \${(pct * 100).toFixed(1)}%,\${it.track} 0)\`);
    const hole = document.createElement("div");
    const w = it.width || 8;
    hole.setAttribute("style", \`left:\${px(w)};top:\${px(w)};width:\${px(it.size - w * 2)};\` +
      \`height:\${px(it.size - w * 2)};border-radius:50%;background:#0000;box-shadow:0 0 0 99px #0000\`);
    return d;
  }

  if (it.t === "bar") {
    const v = Number(it.bind ? get(DATA, it.bind) : it.value) || 0;
    const min = it.min ?? 0, max = it.max ?? 100;
    const pct = Math.min(Math.max((v - min) / (max - min), 0), 1);
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.w)};height:\${px(it.h)};border-radius:\${px(it.h / 2)};background:\${it.track}\`);
    const f = document.createElement("div");
    f.setAttribute("style", \`left:0;top:0;width:\${px(it.w * pct)};height:\${px(it.h)};border-radius:\${px(it.h / 2)};background:\${it.color}\`);
    d.appendChild(f);
    return d;
  }

  if (it.t === "chart") {
    const arr = get(DATA, it.bind);
    const vals = Array.isArray(arr) && arr.length ? arr : [1, 3, 2, 5, 4, 6, 3, 2];
    const lo = Math.min(...vals), hi = Math.max(...vals) || 1;
    const span = hi - lo || 1;
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.w)};height:\${px(it.h)}\`);
    const step = it.w / vals.length;
    vals.forEach((v, i) => {
      const h = Math.max(2, ((v - lo) / span) * it.h);
      const b = document.createElement("div");
      b.setAttribute("style", \`left:\${px(i * step)};top:\${px(it.h - h)};width:\${px(Math.max(1, step - 1))};\` +
        \`height:\${px(h)};background:\${it.color};opacity:.85;border-radius:1px\`);
      d.appendChild(b);
    });
    return d;
  }

  if (it.t === "icon" || it.t === "moon") {
    at(\`left:\${px(it.x)};top:\${px(it.y)};width:\${px(it.size)};height:\${px(it.size)};border-radius:50%;\` +
      \`background:\${it.color};opacity:.9\`);
    return d;
  }

  return d;
}

async function loadData() {
  try { DATA = { ...SAMPLE, ...(await (await fetch("/dash")).json()) }; } catch { DATA = SAMPLE; }
}

async function loadThemes() {
  const info = await (await fetch("/ui/themes")).json();
  hidden = info.hidden;
  const box = document.getElementById("themes");
  box.textContent = "";

  for (const t of info.themes) {
    const preview = await (await fetch("/ui/preview/" + t.id)).json();
    const btn = document.createElement("button");
    btn.className = "theme";
    btn.setAttribute("aria-pressed", String(t.active));
    const frame = document.createElement("div");
    frame.className = "frame";
    frame.appendChild(renderPage(preview.pages[1] || preview.pages[0]));
    btn.appendChild(frame);
    btn.insertAdjacentHTML("beforeend",
      \`<div class="cap">\${t.name}</div><div class="desc">\${t.desc}</div>\`);
    btn.onclick = () => save({ theme: t.id });
    box.appendChild(btn);
  }
}

async function loadPages() {
  doc = await (await fetch("/ui")).json();
  document.getElementById("pagesTitle").textContent = "Telas do tema " + doc.themeName;
  const box = document.getElementById("pages");
  box.textContent = "";

  const all = await (await fetch("/ui/preview/" + doc.theme)).json();
  for (const page of all.pages) {
    const off = hidden.includes(page.id);
    const wrap = document.createElement("div");
    wrap.className = "page" + (off ? " off" : "");
    const frame = document.createElement("div");
    frame.className = "frame";
    frame.appendChild(renderPage(page));
    wrap.appendChild(frame);
    wrap.insertAdjacentHTML("beforeend", \`<div class="lbl">\${page.title}\${off ? " (oculta)" : ""}</div>\`);
    wrap.onclick = () => {
      const next = off ? hidden.filter((id) => id !== page.id) : [...hidden, page.id];
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

async function refresh() {
  await loadThemes();
  await loadPages();
}

(async () => {
  await loadData();
  await refresh();
  status("pronto", false);
})();
</script>
</body>
</html>
`;
