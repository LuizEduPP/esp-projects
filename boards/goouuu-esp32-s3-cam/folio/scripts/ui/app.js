const $ = (id) => document.getElementById(id);

let day = new URLSearchParams(location.search).get("day") || new Date().toISOString().slice(0, 10);
let filter = "all";
let cfgCache = null;
let openAiModels = { chat: [], embed: [], rerank: [], ok: false };
let speakerNames = {};

const PHASE_LABEL = {
  index: "Indexando memória",
  rag: "RAG",
  insights: "Gerando insights",
  error: "Erro",
  done: "Pronto",
};

const LOCALE_OPTS = ["pt-BR", "pt-PT", "en-US", "en-GB", "es-ES", "fr-FR", "de-DE"];
const WHISPER_MODELS = ["tiny", "base", "small", "medium", "large-v3", "large-v3-turbo"];
const WHISPER_LANG_OPTS = [
  { v: "", l: "auto (locale)" }, { v: "pt", l: "português" }, { v: "en", l: "english" },
  { v: "es", l: "español" }, { v: "fr", l: "français" }, { v: "de", l: "deutsch" },
];

const SECTIONS = [
  { title: "Geral", fields: [
    { path: "locale", label: "Idioma", type: "select", options: LOCALE_OPTS },
    { path: "port", label: "Porta HTTP", type: "number" },
    { path: "dataDir", label: "Pasta de dados", type: "text" },
  ]},
  { title: "OpenAI API", fields: [
    { path: "openai.baseUrl", label: "Base URL", type: "text" },
    { path: "openai.apiKey", label: "API key", type: "password" },
    { path: "openai.model", label: "Modelo visão", type: "lmSelect", pool: "chat" },
    { path: "openai.modelDeep", label: "Modelo insights", type: "lmSelect", pool: "chat", emptyOption: "(igual visão)" },
  ]},
  { title: "Áudio", fields: [
    { path: "audio.chunkMs", label: "Chunk (ms)", type: "number" },
    { path: "audio.speechEnergyThreshold", label: "Limiar fala", type: "number", step: "0.001" },
    { path: "audio.ambientEnergyThreshold", label: "Limiar som", type: "number", step: "0.001" },
    { path: "audio.retentionDays", label: "Retenção PCM (dias)", type: "number" },
  ]},
  { title: "Whisper", fields: [
    { path: "audio.whisperModel", label: "Modelo", type: "select", options: WHISPER_MODELS },
    { path: "audio.whisperDevice", label: "Device", type: "select", options: ["cuda", "auto", "cpu", "mps"] },
  ]},
  { title: "Câmera", fields: [
    { path: "frames.captureIntervalMs", label: "Captura (ms)", type: "number" },
    { path: "frames.captionIntervalMs", label: "Caption (ms)", type: "number" },
  ]},
  { title: "Pipeline & insights", fields: [
    { path: "pipeline.enabled", label: "Worker", type: "bool" },
    { path: "pipeline.intervalMs", label: "Worker (ms)", type: "number" },
    { path: "insights.auto", label: "Insights auto", type: "bool" },
    { path: "insights.intervalMs", label: "Insights (ms)", type: "number" },
    { path: "memory.enabled", label: "Memória RAG", type: "bool" },
    { path: "memory.useEmbeddings", label: "Embeddings", type: "bool" },
  ]},
];

function get(obj, path) {
  return path.split(".").reduce((o, k) => o?.[k], obj);
}

function set(obj, path, val) {
  const keys = path.split(".");
  let o = obj;
  for (let i = 0; i < keys.length - 1; i++) {
    if (!o[keys[i]] || typeof o[keys[i]] !== "object") o[keys[i]] = {};
    o = o[keys[i]];
  }
  o[keys[keys.length - 1]] = val;
}

function esc(s) {
  return String(s ?? "").replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;");
}

function toast(msg) {
  const t = $("toast");
  t.textContent = msg;
  t.style.display = "block";
  setTimeout(() => { t.style.display = "none"; }, 4000);
}

function time(iso) {
  try {
    return new Date(iso).toLocaleTimeString("pt-BR", { hour: "2-digit", minute: "2-digit" });
  } catch {
    return iso?.slice(11, 16) || "";
  }
}

function setDay(d) {
  day = d;
  $("day-input").value = d;
  const u = new URL(location.href);
  u.searchParams.set("day", d);
  history.replaceState(null, "", u);
  loadDay();
}

function shiftDay(n) {
  const d = new Date(`${day}T12:00:00`);
  d.setDate(d.getDate() + n);
  setDay(d.toISOString().slice(0, 10));
}

function openDrawer(panel = "system") {
  $("overlay").classList.add("open");
  $("drawer").classList.add("open");
  $("drawer").setAttribute("aria-hidden", "false");
  document.querySelectorAll(".drawer-tab").forEach((b) => b.classList.toggle("on", b.dataset.panel === panel));
  $("panel-system").hidden = panel !== "system";
  $("panel-config").hidden = panel !== "config";
  if (panel === "system") loadSystem();
  if (panel === "config") loadConfigForm();
}

function closeDrawer() {
  $("overlay").classList.remove("open");
  $("drawer").classList.remove("open");
  $("drawer").setAttribute("aria-hidden", "true");
}

function renderInsights(ins) {
  const rt = ins.runtime || {};
  const alert = $("insights-alert");
  if (rt.error) {
    alert.hidden = false;
    alert.textContent = `Insights falhou: ${rt.error}`;
    alert.className = "digest-alert error";
  } else if (rt.busy) {
    alert.hidden = false;
    alert.textContent = PHASE_LABEL[rt.phase] || "Processando…";
    alert.className = "digest-alert";
  } else {
    alert.hidden = true;
  }

  const mins = Math.round((ins.status?.interval_ms || 300000) / 60000);
  $("insights-meta").textContent = ins.updated_at
    ? `Atualizado ${time(ins.updated_at)} · auto ~${mins} min`
    : (ins.status?.needed ? "Aguardando witness / pipeline" : "Sem witness neste dia");

  const st = ins.stats || {};
  $("insights-stats").innerHTML = [
    ["Frames", st.frames ?? 0],
    ["Falas", st.utterances ?? 0],
    ["Sons", Object.values(st.sounds || {}).reduce((a, b) => a + b, 0)],
  ].map(([k, v]) => `<span class="chip">${k} ${v}</span>`).join("");

  const entities = ins.entities || [];
  $("insights-entities").innerHTML = entities.length
    ? entities.map((e) => {
      let pat = {};
      try { pat = JSON.parse(e.patterns_json || "{}"); } catch { /* */ }
      return `<article class="entity-card"><strong>${esc(e.display_name)}</strong>
        <span class="muted">${esc(e.kind)}</span>
        <p class="muted">${esc(pat.notes || pat.utterances_today ? `${pat.utterances_today || 0} falas` : pat.barks_today ? `${pat.barks_today} latidos` : "")}</p></article>`;
    }).join("")
    : '<p class="muted">Nenhuma entidade detectada ainda — faça enroll de voz.</p>';

  const insights = ins.insights || [];
  $("insights-list").innerHTML = insights.length
    ? insights.map((t) => `<li>${esc(t)}</li>`).join("")
    : "<li class='muted'>Insights aparecem quando há witness processado.</li>";

  const patterns = ins.patterns || [];
  $("patterns-list").innerHTML = patterns.length
    ? patterns.map((t) => `<li>${esc(t)}</li>`).join("")
    : "<li class='muted'>—</li>";
}

function renderWitness(items) {
  const list = items.filter((i) => {
    if (filter === "frames") return i.type === "frame";
    if (filter === "speech") return i.type === "audio" && i.speech;
    if (filter === "sound") return i.type === "audio" && !i.speech;
    return i.type === "frame" || i.type === "audio";
  }).sort((a, b) => b.at.localeCompare(a.at));

  if (!list.length) {
    $("witness-feed").innerHTML = '<p class="muted" style="padding:16px">Nada neste filtro.</p>';
    return;
  }

  $("witness-feed").innerHTML = list.map((i) => {
    if (i.type === "frame") {
      return `<article class="witness-item"><time class="witness-time">${time(i.at)}</time>
        <div><div class="witness-kind">Frame</div><img src="/api/frame/${i.frame_id}" alt="" loading="lazy"/>
        <p class="muted">${i.caption ? esc(i.caption) : "Aguardando caption…"}</p></div></article>`;
    }
    const kind = i.speech ? "Fala" : (i.sound_label || "Som");
    const cls = i.speech ? "speech" : "sound";
    const who = i.speaker_id ? (speakerNames[i.speaker_id] || i.speaker_id) : null;
    const txt = i.text
      ? `<blockquote>${who ? `<strong>${esc(who)}:</strong> ` : ""}${esc(i.text)}</blockquote>`
      : (i.processed ? `<p class="muted">${esc(i.sound_label || "Sem transcrição")}</p>` : '<p class="muted">Fila…</p>');
    const aud = i.has_pcm ? `<audio controls preload="none" src="/api/audio/${i.chunk_id}"></audio>` : "";
    return `<article class="witness-item"><time class="witness-time">${time(i.at)}</time>
      <div><div class="witness-kind ${cls}">${esc(kind)}</div>${aud}${txt}</div></article>`;
  }).join("");

  $("witness-feed").querySelectorAll("img").forEach((img) => {
    img.onclick = () => { $("lb-img").src = img.src; $("lightbox").classList.add("open"); };
  });
}

async function loadStatus(ins = {}) {
  const [q, h] = await Promise.all([
    fetch("/api/queue").then((r) => r.json()),
    fetch("/api/health").then((r) => r.json()),
  ]);
  const pa = q.pending?.audio || 0;
  const pf = q.pending?.frames || 0;
  const rt = ins.runtime || {};
  const live = rt.busy ? `<span class="chip live">${PHASE_LABEL[rt.phase] || "insights"}</span>` : "";
  $("status-chips").innerHTML =
    live +
    (pa + pf ? `<span class="chip warn">${pa} áudio · ${pf} frames</span>` : "") +
    `<span class="chip ok">${h.memory_chunks ?? 0} mem</span>`;
}

async function loadDay() {
  const [tl, ins, ent] = await Promise.all([
    fetch(`/api/timeline?day=${day}`).then((r) => r.json()),
    fetch(`/api/insights?day=${day}`).then((r) => r.json()).catch(() => ({})),
    fetch("/api/entities").then((r) => r.json()).catch(() => ({ entities: [] })),
  ]);

  speakerNames = {};
  for (const e of ent.entities || []) {
    if (e.speaker_id) speakerNames[e.speaker_id] = e.display_name;
  }

  const items = tl.items || [];
  const fr = items.filter((i) => i.type === "frame").length;
  const sp = items.filter((i) => i.type === "audio" && i.speech).length;
  const snd = items.filter((i) => i.type === "audio" && !i.speech).length;
  $("witness-stats").textContent = `${fr} frames · ${sp} fala · ${snd} sons`;

  renderInsights(ins);
  renderWitness(items);
  loadStatus(ins);
}

async function loadSystem() {
  const [h, dev] = await Promise.all([
    fetch("/api/health").then((r) => r.json()),
    fetch("/api/devices").then((r) => r.json()),
  ]);
  $("health-grid").innerHTML = [
    ["Dados", h.data_dir], ["Pipeline", h.pipeline ? "ativo" : "pausado"],
    ["Insights", h.insights ? "auto" : "manual"], ["Memória", h.memory_chunks ?? 0],
    ["Fila", `${h.pending?.audio ?? 0} / ${h.pending?.frames ?? 0}`],
  ].map(([k, v]) => `<div class="field"><label>${k}</label><div>${esc(v)}</div></div>`).join("");

  $("devices").innerHTML = (dev.devices || []).length
    ? dev.devices.map((d) => `<div><strong>${esc(d.id)}</strong> · ${d.last_seen_at ? time(d.last_seen_at) : "—"}</div>`).join("")
    : '<p class="muted">Nenhum ESP.</p>';

  $("mem-stats").textContent = `${h.memory_chunks ?? 0} chunks indexados`;
}

function buildConfigForm() {
  $("cfg-form").innerHTML = SECTIONS.map((sec) =>
    `<h3>${sec.title}</h3><div class="grid2">${sec.fields.map((f) => {
      const id = `f-${f.path.replace(/\./g, "-")}`;
      if (f.type === "bool") {
        return `<div class="field"><label for="${id}">${f.label}</label>
          <select id="${id}" data-path="${f.path}"><option value="true">sim</option><option value="false">não</option></select></div>`;
      }
      if (f.type === "lmSelect") {
        return `<div class="field"><label for="${id}">${f.label}</label>
          <select id="${id}" data-path="${f.path}" data-lm-pool="${f.pool}" data-empty="${f.emptyOption || ""}"></select></div>`;
      }
      if (f.type === "select") {
        const opts = (f.labels || f.options.map((o) => ({ v: o, l: o })))
          .map((o) => `<option value="${typeof o === "string" ? o : o.v}">${typeof o === "string" ? o : o.l}</option>`).join("");
        return `<div class="field"><label for="${id}">${f.label}</label><select id="${id}" data-path="${f.path}">${opts}</select></div>`;
      }
      return `<div class="field"><label for="${id}">${f.label}</label>
        <input id="${id}" data-path="${f.path}" type="${f.type || "text"}" ${f.step ? `step="${f.step}"` : ""}/></div>`;
    }).join("")}</div>`,
  ).join("");
}

function fillLmSelects() {
  document.querySelectorAll("[data-lm-pool]").forEach((sel) => {
    const pool = sel.dataset.lmPool;
    const cur = sel.value;
    const list = openAiModels[pool] || [];
    let html = sel.dataset.empty ? `<option value="">${sel.dataset.empty}</option>` : "";
    html += list.map((id) => `<option value="${esc(id)}">${esc(id)}</option>`).join("");
    if (!list.length) html += '<option value="" disabled>(offline)</option>';
    sel.innerHTML = html;
    if (cur) sel.value = cur;
  });
}

async function refreshOpenAiModels() {
  const r = await fetch("/api/openai/models").then((x) => x.json());
  if (r.ok) {
    openAiModels = r;
    $("cfg-openai-status").textContent = `${r.chat.length} modelos`;
    fillLmSelects();
  }
}

function applyConfigToForm(cfg) {
  document.querySelectorAll("[data-path]").forEach((el) => {
    const v = get(cfg, el.dataset.path);
    if (el.tagName === "SELECT" && el.querySelector('option[value="true"]') && !el.dataset.lmPool) {
      el.value = v === false || v === "false" ? "false" : "true";
    } else {
      el.value = v == null ? "" : v;
    }
  });
  fillLmSelects();
}

function loadConfigForm() {
  fetch("/api/config").then((r) => r.json()).then(async (cfg) => {
    cfgCache = cfg;
    $("cfg-info").textContent = cfg.configPath || "defaults";
    applyConfigToForm(cfg);
    await refreshOpenAiModels();
    applyConfigToForm(cfg);
  });
}

$("day-input").value = day;
$("day-prev").onclick = () => shiftDay(-1);
$("day-next").onclick = () => shiftDay(1);
$("day-input").onchange = (e) => setDay(e.target.value);

document.querySelectorAll(".filter").forEach((btn) => {
  btn.onclick = () => {
    document.querySelectorAll(".filter").forEach((b) => b.classList.remove("on"));
    btn.classList.add("on");
    filter = btn.dataset.f;
    loadDay();
  };
});

$("btn-drawer").onclick = () => openDrawer("system");
$("drawer-close").onclick = closeDrawer;
$("overlay").onclick = closeDrawer;
document.querySelectorAll(".drawer-tab").forEach((b) => { b.onclick = () => openDrawer(b.dataset.panel); });

$("btn-process").onclick = async () => {
  $("btn-process").disabled = true;
  try {
    await fetch("/api/process", { method: "POST" });
    toast("Fila processada");
    loadDay();
  } catch (e) { toast(e.message); }
  $("btn-process").disabled = false;
};

$("btn-reindex").onclick = async () => {
  const r = await fetch("/api/memory/reindex", { method: "POST" }).then((x) => x.json());
  toast(`Memória: ${r.before} → ${r.after}`);
  loadSystem();
};

$("mem-search").onclick = async () => {
  const q = $("mem-q").value;
  const r = await fetch(`/api/memory?day=${day}&q=${encodeURIComponent(q)}`).then((x) => x.json());
  $("mem-hits").innerHTML = (r.hits || []).map((h) =>
    `<div class="mem-hit"><strong>${esc(h.day)}</strong> ${esc(h.kind)}<br>${esc(h.text)}</div>`,
  ).join("") || '<p class="muted">Nenhum hit.</p>';
};

buildConfigForm();
$("cfg-form").onsubmit = async (e) => {
  e.preventDefault();
  const patch = JSON.parse(JSON.stringify(cfgCache));
  document.querySelectorAll("[data-path]").forEach((el) => {
    let v = el.value;
    if (el.tagName === "SELECT" && el.querySelector('option[value="true"]')) v = v === "true";
    else if (el.type === "number") v = v === "" ? null : Number(v);
    else if (v === "") v = null;
    set(patch, el.dataset.path, v);
  });
  delete patch.configPath; delete patch.version; delete patch.runtime;
  await fetch("/api/config", { method: "PUT", headers: { "Content-Type": "application/json" }, body: JSON.stringify(patch) });
  toast("Config salva");
};
$("cfg-reload").onclick = loadConfigForm;
$("cfg-openai-refresh").onclick = refreshOpenAiModels;
$("lb-close").onclick = () => $("lightbox").classList.remove("open");

loadDay();
setInterval(loadDay, 5000);
