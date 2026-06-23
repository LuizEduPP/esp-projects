const $ = (id) => document.getElementById(id);

let day = new URLSearchParams(location.search).get("day") || new Date().toISOString().slice(0, 10);
let filter = "all";
let cfgCache = null;
let openAiModels = { chat: [], embed: [], rerank: [], ok: false };
let witnessIndex = new Map();

const PHASE_LABEL = {
  episodes: "Episódios",
  pass_a: "Pass A · fatos",
  pass_b: "Pass B · interpretação",
  pass_c: "Pass C · crítica",
  pass_d: "Pass D · crônica",
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
    { path: "port", label: "Porta HTTP", type: "number", hint: "Reinicia brain" },
    { path: "dataDir", label: "Pasta de dados", type: "text", hint: "Reinicia brain" },
  ]},
  { title: "OpenAI API", fields: [
    { path: "openai.baseUrl", label: "Base URL", type: "text", hint: "ex.: http://127.0.0.1:1234/v1" },
    { path: "openai.apiKey", label: "API key", type: "password", hint: "opcional (servidor local)" },
    { path: "openai.model", label: "Modelo rápido", type: "lmSelect", pool: "chat" },
    { path: "openai.modelDeep", label: "Modelo deep", type: "lmSelect", pool: "chat", emptyOption: "(igual ao rápido)" },
  ]},
  { title: "Áudio", fields: [
    { path: "audio.chunkMs", label: "Chunk (ms)", type: "number" },
    { path: "audio.sampleRate", label: "Sample rate", type: "number" },
    { path: "audio.speechEnergyThreshold", label: "Limiar fala", type: "number", step: "0.001" },
    { path: "audio.retentionDays", label: "Retenção PCM (dias)", type: "number" },
    { path: "audio.pipelineBatch", label: "Batch Whisper", type: "number" },
  ]},
  { title: "Whisper", fields: [
    { path: "audio.whisperBin", label: "Binário", type: "text" },
    { path: "audio.whisperModel", label: "Modelo", type: "select", options: WHISPER_MODELS },
    { path: "audio.whisperDevice", label: "Device", type: "select", options: ["cuda", "auto", "cpu", "mps"] },
    { path: "audio.whisperTimeoutMs", label: "Timeout (ms)", type: "number" },
    { path: "audio.whisperLanguage", label: "Idioma STT", type: "select", options: WHISPER_LANG_OPTS.map((o) => o.v), labels: WHISPER_LANG_OPTS },
  ]},
  { title: "Câmera", fields: [
    { path: "frames.captureIntervalMs", label: "Captura (ms)", type: "number" },
    { path: "frames.captionIntervalMs", label: "Caption (ms)", type: "number" },
    { path: "frames.jpegQuality", label: "JPEG quality", type: "number" },
    { path: "frames.size", label: "Resolução", type: "select", options: ["CIF", "QVGA", "VGA", "SVGA", "XGA"] },
  ]},
  { title: "Pipeline & digest", fields: [
    { path: "pipeline.enabled", label: "Pipeline", type: "bool" },
    { path: "pipeline.intervalMs", label: "Worker (ms)", type: "number" },
    { path: "digest.auto", label: "Digest auto", type: "bool" },
    { path: "digest.intervalMs", label: "Digest (ms)", type: "number" },
    { path: "digest.passDTemperature", label: "Pass D temp", type: "number", step: "0.01" },
  ]},
  { title: "Memória", fields: [
    { path: "memory.enabled", label: "RAG ativa", type: "bool" },
    { path: "memory.lookbackDays", label: "Lookback", type: "number" },
    { path: "memory.useEmbeddings", label: "Embeddings", type: "bool" },
    { path: "memory.rerank.enabled", label: "Rerank", type: "bool" },
    { path: "memory.rerank.model", label: "Modelo rerank", type: "lmSelect", pool: "rerank", emptyOption: "(escolher…)" },
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
    return new Date(iso).toLocaleTimeString("pt-BR", { hour: "2-digit", minute: "2-digit", second: "2-digit" });
  } catch {
    return iso?.slice(11, 19) || "";
  }
}

function todayStr() {
  return new Date().toISOString().slice(0, 10);
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

function scrollToEvidence(id) {
  const el = witnessIndex.get(id);
  if (!el) return;
  el.classList.add("highlight");
  el.scrollIntoView({ behavior: "smooth", block: "center" });
  setTimeout(() => el.classList.remove("highlight"), 2400);
}

function linkEvidenceIds(text) {
  return esc(text).replace(/\b(utt:\d+|frm:\d+)\b/g, (m) =>
    `<a href="#" data-evidence="${m}">${m}</a>`);
}

function renderChronicleHtml(prose) {
  const raw = String(prose ?? "").trim();
  if (!raw) return { title: "", body: "" };

  const lines = raw.split("\n");
  let title = "";
  let rest = raw;

  if (/^folio\s·/i.test(lines[0]?.trim())) {
    title = lines[0].trim();
    rest = lines.slice(1).join("\n").trim();
  }

  const evidenceMatch = rest.match(/\n*(Evid[eê]ncia|Evidence|Preuves|Belege)\s*:\s*(.+)$/is);
  let bodyText = rest;
  let evidenceHtml = "";

  if (evidenceMatch) {
    bodyText = rest.slice(0, evidenceMatch.index).trim();
    const label = evidenceMatch[1];
    const ids = evidenceMatch[2].trim();
    evidenceHtml = `<p class="evidence">${esc(label)}: ${linkEvidenceIds(ids)}</p>`;
  }

  const paragraphs = bodyText.split(/\n{2,}/).filter(Boolean)
    .map((p) => `<p>${esc(p).replace(/\n/g, "<br>")}</p>`)
    .join("");

  return { title, body: paragraphs + evidenceHtml };
}

function showDigestAlert(msg, kind = "warn") {
  const el = $("digest-alert");
  if (!msg) {
    el.hidden = true;
    el.textContent = "";
    return;
  }
  el.hidden = false;
  el.textContent = msg;
  el.className = `digest-alert${kind === "error" ? " error" : ""}`;
}

function renderChronicle(d, st, items) {
  const isToday = day === todayStr();
  const mins = Math.max(1, Math.round((st.interval_ms || 300000) / 60000));
  const hasWitness = (st.stats && (st.stats.frames + st.stats.utterances + st.stats.speech > 0))
    || items.some((i) => i.type === "frame" || (i.type === "audio" && i.speech));

  const rt = st.runtime || {};
  const busy = rt.busy || st.run;

  if (rt.error) {
    showDigestAlert(`Digest falhou: ${rt.error}`, "error");
  } else if (busy) {
    showDigestAlert(`Gerando crônica — ${PHASE_LABEL[rt.phase] || rt.phase || "…"}`);
  } else {
    showDigestAlert(null);
  }

  $("chronicle-title").textContent = "";
  $("chronicle-body").className = "chronicle-body";

  const prose = d.prose?.trim();
  const draft = d.draft?.trim();
  const displayText = prose || (draft ? `Folio · ${day}\n\n${draft}` : null);
  const isDraft = !prose && Boolean(draft);

  if (displayText) {
    const { title, body } = renderChronicleHtml(displayText);
    $("chronicle-title").textContent = title || `Folio · ${day}`;
    $("chronicle-body").innerHTML = body;
    $("chronicle-body").classList.toggle("is-draft", isDraft || d.is_draft);
    $("chronicle-body").querySelectorAll("[data-evidence]").forEach((a) => {
      a.onclick = (e) => {
        e.preventDefault();
        scrollToEvidence(a.dataset.evidence);
      };
    });
    $("chronicle-meta").textContent = isDraft
      ? `Rascunho · ${busy ? PHASE_LABEL[rt.phase] || "processando" : time(d.updated_at) || "agora"}`
      : (isToday
        ? `Crônica · ${time(d.updated_at)} · refresh ~${mins} min`
        : `Arquivo · ${time(d.updated_at)}`);
    return;
  }

  $("chronicle-body").className = "chronicle-body chronicle-empty";
  $("chronicle-title").textContent = isToday ? "Hoje" : day;

  if (!hasWitness) {
    $("chronicle-body").textContent = isToday
      ? "Aguardando witness do ESP — mic e câmera passivos."
      : "Nenhuma crônica para este dia.";
    $("chronicle-meta").textContent = isToday ? "Testemunha passiva" : "";
    return;
  }

  if (busy) {
    $("chronicle-body").textContent = `Interpretando o dia (${PHASE_LABEL[rt.phase] || "digest"})…`;
  } else {
    $("chronicle-body").textContent = `Witness ok — crônica automática a cada ~${mins} min.`;
  }
  $("chronicle-meta").textContent = busy ? "Digest em execução" : `Automático · ~${mins} min`;
}

function renderWitness(items) {
  witnessIndex = new Map();
  const list = items.filter((i) => {
    if (filter === "frames") return i.type === "frame";
    if (filter === "speech") return i.type === "audio" && i.speech;
    return i.type === "frame" || (i.type === "audio" && i.speech);
  }).sort((a, b) => b.at.localeCompare(a.at));

  if (!list.length) {
    $("witness-feed").innerHTML = '<p class="muted" style="padding:16px">Nada neste filtro.</p>';
    return;
  }

  $("witness-feed").innerHTML = list.map((i) => {
    if (i.type === "frame") {
      const ev = `frm:${i.frame_id}`;
      return `<article class="witness-item" data-ev="${ev}" id="ev-${ev}">
        <time class="witness-time">${time(i.at)}</time>
        <div><div class="witness-kind">Frame</div>
        <img src="/api/frame/${i.frame_id}" alt="" loading="lazy"/>
        <p class="muted">${i.caption ? esc(i.caption) : "Aguardando caption…"}</p></div></article>`;
    }
    const ev = i.utterance_id ? `utt:${i.utterance_id}` : `aud:${i.chunk_id}`;
    const txt = i.text
      ? `<blockquote>${esc(i.text)}</blockquote>`
      : (i.processed ? '<p class="muted">Sem fala</p>' : '<p class="muted">Fila Whisper…</p>');
    const aud = i.has_pcm
      ? `<audio controls preload="none" src="/api/audio/${i.chunk_id}"></audio>`
      : '<p class="muted">PCM expirado</p>';
    return `<article class="witness-item" data-ev="${ev}" id="ev-${ev}">
      <time class="witness-time">${time(i.at)}</time>
      <div><div class="witness-kind speech">Fala</div>${aud}${txt}</div></article>`;
  }).join("");

  $("witness-feed").querySelectorAll(".witness-item").forEach((el) => {
    witnessIndex.set(el.dataset.ev, el);
    const utt = el.dataset.ev.match(/^utt:(\d+)$/);
    if (utt) witnessIndex.set(`utt:${utt[1]}`, el);
  });

  $("witness-feed").querySelectorAll("img").forEach((img) => {
    img.onclick = () => {
      $("lb-img").src = img.src;
      $("lightbox").classList.add("open");
    };
  });
}

async function loadStatus(st = {}) {
  const [q, h] = await Promise.all([
    fetch("/api/queue").then((r) => r.json()),
    fetch("/api/health").then((r) => r.json()),
  ]);
  const pa = q.pending?.audio || 0;
  const pf = q.pending?.frames || 0;
  const rt = st.runtime || {};
  const live = rt.busy ? `<span class="chip live">${PHASE_LABEL[rt.phase] || "digest"}</span>` : "";
  $("status-chips").innerHTML =
    live +
    (pa + pf ? `<span class="chip warn">${pa} áudio · ${pf} frames</span>` : "") +
    `<span class="chip ok">${h.memory_chunks ?? 0} mem</span>` +
    `<span class="chip">:${location.port || "__PORT__"}</span>`;
}

async function loadDay() {
  const [tl, d, st] = await Promise.all([
    fetch(`/api/timeline?day=${day}`).then((r) => r.json()),
    fetch(`/api/digest?day=${day}`).then((r) => r.json()).catch(() => ({})),
    fetch(`/api/digest/status?day=${day}`).then((r) => r.json()).catch(() => ({})),
  ]);
  const items = tl.items || [];
  const fr = items.filter((i) => i.type === "frame").length;
  const sp = items.filter((i) => i.type === "audio" && i.speech).length;
  $("witness-stats").textContent = `${fr} frames · ${sp} fala`;
  renderChronicle(d, st, items);
  renderWitness(items);
  loadStatus(st);
}

async function loadSystem() {
  const [h, dev, mem] = await Promise.all([
    fetch("/api/health").then((r) => r.json()),
    fetch("/api/devices").then((r) => r.json()),
    fetch(`/api/memory?day=${day}`).then((r) => r.json()),
  ]);
  $("health-grid").innerHTML = [
    ["Dados", h.data_dir], ["Pipeline", h.pipeline ? "ativo" : "pausado"],
    ["Fila áudio", h.pending?.audio ?? 0], ["Fila frames", h.pending?.frames ?? 0],
    ["Memória", h.memory_chunks ?? 0], ["Porta", h.port],
  ].map(([k, v]) => `<div class="field"><label>${k}</label><div>${esc(v)}</div></div>`).join("");

  $("devices").innerHTML = (dev.devices || []).length
    ? dev.devices.map((d) => {
      const ok = d.node_config_applied === dev.brain_config_version;
      return `<div class="${ok ? "ok" : "bad"}"><strong>${esc(d.id)}</strong> · ${d.last_seen_at ? time(d.last_seen_at) : "—"} ${ok ? "✓" : "≠ config"}</div>`;
    }).join("")
    : '<p class="muted">Nenhum ESP — envie áudio/frame.</p>';

  $("mem-stats").textContent = `${mem.chunks ?? 0} chunks`;
  if (mem.hits?.length) {
    $("mem-hits").innerHTML = mem.hits.map((h) =>
      `<div class="mem-hit"><strong>${esc(h.day)}</strong> ${esc(h.kind)} · ${(h.rerank_score ?? h.score)?.toFixed?.(2)}<br>${esc(h.text)}</div>`,
    ).join("");
  }
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
    const list = openAiModels[pool]?.length ? openAiModels[pool]
      : (pool === "rerank" ? (openAiModels.all || []).filter((id) => /rerank/i.test(id)) : []);
    let html = sel.dataset.empty ? `<option value="">${sel.dataset.empty}</option>` : "";
    html += list.map((id) => `<option value="${esc(id)}">${esc(id)}</option>`).join("");
    if (!list.length) html += '<option value="" disabled>(OpenAI API offline)</option>';
    sel.innerHTML = html;
    if (cur) sel.value = cur;
  });
}

async function refreshOpenAiModels() {
  $("cfg-openai-status").textContent = "Conectando…";
  const r = await fetch("/api/openai/models").then((x) => x.json());
  if (r.ok) {
    openAiModels = r;
    $("cfg-openai-status").textContent = `${r.chat.length} chat · ${r.embed.length} embed · ${r.baseUrl || ""}`;
    fillLmSelects();
  } else {
    $("cfg-openai-status").textContent = `Erro: ${r.error}`;
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
    $("cfg-info").textContent = `${cfg.configPath || "defaults"} · ${cfg.version || "?"}`;
    const rt = cfg.runtime || {};
    const m = rt.models || {};
    $("cfg-runtime").textContent = `Whisper ${rt.whisperDeviceEffective} · fast=${m.fast} · deep=${m.deep}`;
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

document.querySelectorAll(".drawer-tab").forEach((btn) => {
  btn.onclick = () => openDrawer(btn.dataset.panel);
});

$("btn-process").onclick = async () => {
  $("btn-process").disabled = true;
  try {
    const r = await fetch("/api/process", { method: "POST" }).then((x) => x.json());
    const utt = (r.audio || []).filter((x) => x.text).length;
    $("action-msg").textContent = `${utt} transcrições · fila ${r.pending?.audio}/${r.pending?.frames}`;
    loadDay();
  } catch (e) {
    $("action-msg").textContent = e.message;
  }
  $("btn-process").disabled = false;
};

$("btn-reindex").onclick = async () => {
  $("btn-reindex").disabled = true;
  try {
    const r = await fetch("/api/memory/reindex", { method: "POST" }).then((x) => x.json());
    toast(`Memória: ${r.before} → ${r.after}`);
    loadSystem();
  } catch (e) {
    toast(e.message);
  }
  $("btn-reindex").disabled = false;
};

$("mem-search").onclick = async () => {
  const q = $("mem-q").value;
  const r = await fetch(`/api/memory?day=${day}&q=${encodeURIComponent(q)}`).then((x) => x.json());
  $("mem-hits").innerHTML = (r.hits || []).length
    ? r.hits.map((h) => `<div class="mem-hit"><strong>${esc(h.day)}</strong> ${esc(h.kind)}<br>${esc(h.text)}</div>`).join("")
    : '<p class="muted">Nenhum hit.</p>';
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
  delete patch.configPath;
  delete patch.version;
  delete patch.runtime;
  const r = await fetch("/api/config", {
    method: "PUT",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(patch),
  }).then((x) => x.json());
  $("cfg-save-msg").textContent = r.restartRecommended ? "Salvo — reinicie brain." : "Salvo.";
  cfgCache = r.config || patch;
  toast("Config salva");
};

$("cfg-reload").onclick = loadConfigForm;
$("cfg-openai-refresh").onclick = refreshOpenAiModels;
$("lb-close").onclick = () => $("lightbox").classList.remove("open");
$("lightbox").onclick = (e) => { if (e.target.id === "lightbox") $("lightbox").classList.remove("open"); };

loadDay();
setInterval(loadDay, 5000);
