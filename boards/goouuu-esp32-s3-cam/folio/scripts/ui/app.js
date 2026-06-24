const $ = (id) => document.getElementById(id);

let day = new URLSearchParams(location.search).get("day") || new Date().toISOString().slice(0, 10);
let filter = "all";
let cfgCache = null;
let lmModels = { chat: [], embed: [], rerank: [], ok: false };
let timelineGroups = [];
let speakerNames = {};

function humanDayMeta(iso) {
  if (!iso) return "";
  try {
    const d = new Date(iso);
    const diff = Date.now() - d.getTime();
    if (diff < 120_000) return "agora há pouco";
    if (diff < 3_600_000) return `há ${Math.round(diff / 60_000)} min`;
    return d.toLocaleTimeString("pt-BR", { hour: "2-digit", minute: "2-digit" });
  } catch {
    return time(iso);
  }
}

function entityBlurb(e, summary = "") {
  let pat = {};
  try { pat = JSON.parse(e.patterns_json || "{}"); } catch { /* */ }
  if (pat.utterances_today) return `${pat.utterances_today} falas hoje`;
  if (pat.barks_today) return `${pat.barks_today} latidos`;
  if (pat.notes) {
    const notes = String(pat.notes).trim().replace(/\s+/g, " ");
    if (summary && notes.length > 40 && summary.includes(notes.slice(0, 80))) {
      return "";
    }
    return notes.length > 100 ? `${notes.slice(0, 97)}…` : notes;
  }
  return e.kind === "person" || e.kind === "group" ? "por aqui" : "";
}

const PHASE_LABEL = {
  index: "Organizando memória…",
  rag: "Lembrando do passado…",
  insights: "Entendendo o dia…",
  error: "Algo deu errado",
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
    { path: "dataDir", label: "Pasta de dados", type: "text", placeholder: "~/.folio" },
  ]},
  { title: "LM Studio", hint: "Visão, insights, embeddings e rerank — só LM Studio (Whisper é separado)", fields: [
    { path: "lm.url", label: "URL", type: "text", placeholder: "http://127.0.0.1:1234/v1" },
    { path: "lm.model", label: "Modelo visão", type: "lmSelect", pool: "chat" },
    { path: "lm.modelDeep", label: "Modelo insights", type: "lmSelect", pool: "chat", emptyOption: "(igual visão)" },
    { path: "lm.modelEmbed", label: "Modelo embed", type: "lmSelect", pool: "embed", emptyOption: "(desligado)" },
    { path: "lm.modelRerank", label: "Modelo rerank", type: "lmSelect", pool: "rerank", emptyOption: "(desligado)" },
  ]},
  { title: "Áudio · VAD", hint: "Grava só quando detecta voz (estilo Omi)", fields: [
    { path: "audio.vad.frameMs", label: "Frame (ms)", type: "number" },
    { path: "audio.speechEnergyThreshold", label: "Limiar voz", type: "number", step: "0.001" },
    { path: "audio.vad.debounceMs", label: "Debounce (ms)", type: "number" },
    { path: "audio.vad.silenceMs", label: "Silêncio p/ parar (ms)", type: "number" },
    { path: "audio.vad.prerollMs", label: "Pré-roll (ms)", type: "number" },
    { path: "audio.retentionDays", label: "Retenção PCM (dias)", type: "number" },
  ]},
  { title: "Whisper (STT local)", hint: "Transcrição de voz — CLI openai-whisper, independente do LM Studio", fields: [
    { path: "audio.whisperModel", label: "Modelo", type: "select", options: WHISPER_MODELS },
    { path: "audio.whisperDevice", label: "Device", type: "select", options: ["cuda", "auto", "cpu", "mps"] },
    { path: "audio.whisperLanguage", label: "Idioma STT", type: "select", options: WHISPER_LANG_OPTS.map((o) => o.v), labels: WHISPER_LANG_OPTS },
  ]},
  { title: "Câmera", fields: [
    { path: "frames.captureIntervalMs", label: "Captura (ms)", type: "number" },
    { path: "frames.captionIntervalMs", label: "Caption (ms)", type: "number" },
    { path: "frames.jpegQuality", label: "JPEG quality", type: "number" },
    { path: "frames.size", label: "Resolução", type: "select", options: ["QVGA", "VGA", "SVGA", "CIF"] },
  ]},
  { title: "Percepção", hint: "Movimento, brilho, objetos e sons — sem API externa", fields: [
    { path: "perception.motionMin", label: "Movimento mín.", type: "number", step: "0.001" },
    { path: "perception.motionForceMs", label: "Forçar análise (ms)", type: "number" },
    { path: "perception.autoEnhance", label: "Auto brilho", type: "bool" },
    { path: "perception.vision.jpegQuality", label: "JPEG p/ LM", type: "number" },
    { path: "perception.vision.maxPasses", label: "Passes imagem", type: "number" },
    { path: "perception.vision.targetBrightness", label: "Brilho alvo", type: "number", step: "0.01" },
    { path: "perception.vision.maxGain", label: "Ganho máx.", type: "number", step: "0.01" },
    { path: "perception.storeSounds", label: "Guardar sons", type: "bool" },
    { path: "perception.soundMinEnergy", label: "Energia som mín.", type: "number", step: "0.001" },
    { path: "perception.soundMinConfidence", label: "Confiança som mín.", type: "number", step: "0.05" },
    { path: "perception.soundEngine", label: "Motor de som", type: "select", options: ["yamnet", "heuristic"] },
    { path: "perception.yamnetMinScore", label: "YAMNet score mín.", type: "number", step: "0.05" },
  ]},
  { title: "Pipeline", fields: [
    { path: "pipeline.enabled", label: "Worker", type: "bool" },
    { path: "pipeline.intervalMs", label: "Worker (ms)", type: "number" },
    { path: "insights.auto", label: "Insights auto", type: "bool" },
    { path: "insights.intervalMs", label: "Insights (ms)", type: "number" },
    { path: "insights.temperature", label: "Insights temp", type: "number", step: "0.05" },
  ]},
  { title: "Memória", hint: "Busca lexical local; embeddings via LM Studio (lm.modelEmbed)", fields: [
    { path: "memory.enabled", label: "RAG ativo", type: "bool" },
    { path: "memory.useEmbeddings", label: "Embeddings (LM Studio)", type: "bool" },
    { path: "memory.contextQueryTemplate", label: "Query contexto ({day})", type: "text" },
    { path: "memory.lexical.minTokenLength", label: "Token mín. (lexical)", type: "number" },
    { path: "memory.retrieveLimit", label: "Hits por busca", type: "number" },
  ]},
  { title: "ESP32", fields: [
    { path: "node.wifiRetryMs", label: "WiFi retry (ms)", type: "number" },
    { path: "node.statusIntervalMs", label: "Poll config (ms)", type: "number" },
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
    alert.textContent = `Não consegui entender o dia: ${rt.error}`;
    alert.className = "digest-alert error";
  } else if (rt.busy) {
    alert.hidden = false;
    alert.textContent = PHASE_LABEL[rt.phase] || "Pensando…";
    alert.className = "digest-alert";
  } else {
    alert.hidden = true;
  }

  $("insights-meta").textContent = ins.updated_at
    ? humanDayMeta(ins.updated_at)
    : (ins.status?.needed ? "Coletando momentos…" : "Dia tranquilo até agora");

  const st = ins.stats || {};
  const speech = st.utterances ?? 0;
  const scenes = st.scenes ?? st.frames ?? 0;
  const summaryEl = $("insights-summary");
  if (ins.summary) {
    summaryEl.textContent = ins.summary;
    summaryEl.classList.remove("muted");
  } else if (speech + scenes > 0) {
    summaryEl.textContent =
      speech && scenes
        ? `${scenes} cena${scenes > 1 ? "s" : ""} e ${speech} fala${speech > 1 ? "s" : ""} — gerando resumo…`
        : speech
          ? `${speech} fala${speech > 1 ? "s" : ""} capturada${speech > 1 ? "s" : ""}.`
          : `${scenes} cena${scenes > 1 ? "s" : ""} do ambiente.`;
    summaryEl.classList.add("muted");
  } else {
    summaryEl.textContent = "O ambiente está quieto. Quando houver voz ou movimento, aparece aqui.";
    summaryEl.classList.add("muted");
  }

  const moments = ins.moments || [];
  $("insights-moments").innerHTML = moments.length
    ? moments.map((m) => `<div class="moment-chip">${esc(m)}</div>`).join("")
    : "";

  const entities = ins.entities || [];
  $("insights-entities").innerHTML = entities.length
    ? entities.map((e) => {
      const blurb = entityBlurb(e, ins.summary || "");
      return `<article class="entity-card"><strong>${esc(e.display_name)}</strong>${
        blurb ? `<p class="muted">${esc(blurb)}</p>` : ""
      }</article>`;
    }).join("")
    : "";

  const insights = ins.insights || [];
  const patterns = ins.patterns || [];
  $("insights-list").innerHTML = insights.map((t) => `<li>${esc(t)}</li>`).join("");
  $("patterns-list").innerHTML = patterns.map((t) => `<li>${esc(t)}</li>`).join("");
  $("insights-more").open = insights.length + patterns.length > 0;
}

function renderWitnessGroups(groups) {
  const list = groups.filter((g) => {
    if (filter === "frames") return g.type === "scene" || g.type === "frame_pending";
    if (filter === "speech") return g.type === "speech";
    if (filter === "sounds") return g.type === "sound";
    return true;
  });

  if (!list.length) {
    $("witness-feed").innerHTML = '<p class="empty-feed">Nada por aqui ainda.</p>';
    return;
  }

  $("witness-feed").innerHTML = list.map((g) => {
    const hourMark = g.showHour
      ? `<div class="hour-mark">${esc(g.hour_label)}</div>`
      : "";

    if (g.type === "speech") {
      const who = g.speaker_id ? (speakerNames[g.speaker_id] || g.speaker_id) : null;
      const lines = g.lines.map((ln) => {
        const aud = ln.has_pcm
          ? `<audio controls preload="none" src="/api/audio/${ln.chunk_id}"></audio>`
          : "";
        return `<p class="speech-line">${aud}${esc(ln.text)}</p>`;
      }).join("");
      return `${hourMark}<article class="witness-group speech">
        <header>${who ? esc(who) : "Alguém falou"} · ${humanDayMeta(g.at)}</header>
        ${lines}
      </article>`;
    }

    if (g.type === "sound") {
      const repeat = g.count > 1 ? `<span class="repeat-note">· ${g.count}×</span>` : "";
      const cid = g.chunk_ids?.[g.chunk_ids.length - 1];
      const aud = cid
        ? `<audio controls preload="none" src="/api/audio/${cid}"></audio>`
        : "";
      return `${hourMark}<article class="witness-group sound">
        <header><span class="badge sound">${esc(g.sound_label || "Som")}</span> · ${humanDayMeta(g.at)}${repeat}</header>
        ${aud}
      </article>`;
    }

    if (g.type === "scene") {
      const fid = g.frame_ids[g.frame_ids.length - 1];
      const repeat = g.count > 1 ? `<span class="repeat-note">· ${g.count}×</span>` : "";
      const scene = g.scene_json ? (() => { try { return JSON.parse(g.scene_json); } catch { return null; } })() : null;
      const motion = scene?.motion_level === "high"
        ? `<span class="badge motion">movimento</span> `
        : "";
      return `${hourMark}<article class="witness-group scene">
        <header>${motion}Cena · ${humanDayMeta(g.at)}${repeat}</header>
        <img src="/api/frame/${fid}" alt="" loading="lazy"/>
        <p>${esc(g.caption)}</p>
      </article>`;
    }

    if (filter !== "frames") {
      return "";
    }

    const fid = g.frame_ids?.[g.frame_ids.length - 1];
    const n = g.count ?? g.frame_ids?.length ?? 1;
    const header = n > 1 ? `${n} imagens na fila…` : "Processando imagem…";
    return `${hourMark}<article class="witness-group pending">
      <header>${esc(header)}</header>
      ${fid ? `<img src="/api/frame/${fid}" alt="" loading="lazy" class="dim"/>` : ""}
    </article>`;
  }).filter(Boolean).join("");

  $("witness-feed").querySelectorAll("img:not(.dim)").forEach((img) => {
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
  const live = rt.busy ? `<span class="chip live">${PHASE_LABEL[rt.phase] || "pensando"}</span>` : "";
  $("status-chips").innerHTML =
    live +
    (pa + pf ? `<span class="chip warn">${pa + pf} na fila</span>` : "");
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

  timelineGroups = tl.groups || [];
  const sceneCount = tl.scenes ?? timelineGroups.filter((g) => g.type === "scene").length;
  const speechCount = timelineGroups.filter((g) => g.type === "speech").length;
  $("witness-stats").textContent =
    timelineGroups.length
      ? `${timelineGroups.length} momento${timelineGroups.length > 1 ? "s" : ""}` +
        (sceneCount || speechCount ? ` · ${sceneCount} cena${sceneCount !== 1 ? "s" : ""}, ${speechCount} fala${speechCount !== 1 ? "s" : ""}` : "")
      : "Aguardando…";

  renderInsights(ins);
  renderWitnessGroups(timelineGroups);
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
  $("cfg-form").innerHTML = SECTIONS.map((sec) => {
    const hint = sec.hint ? `<p class="muted cfg-hint">${esc(sec.hint)}</p>` : "";
    const fields = sec.fields.map((f) => {
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
      const ph = f.placeholder ? ` placeholder="${esc(f.placeholder)}"` : "";
      return `<div class="field"><label for="${id}">${f.label}</label>
        <input id="${id}" data-path="${f.path}" type="${f.type || "text"}"${ph} ${f.step ? `step="${f.step}"` : ""}/></div>`;
    }).join("");
    return `<section class="cfg-section"><h3>${sec.title}</h3>${hint}<div class="grid2">${fields}</div></section>`;
  }).join("");
}

function fillLmSelects() {
  document.querySelectorAll("[data-lm-pool]").forEach((sel) => {
    const pool = sel.dataset.lmPool;
    const cur = sel.value;
    const list = lmModels[pool] || [];
    let html = sel.dataset.empty ? `<option value="">${sel.dataset.empty}</option>` : "";
    html += list.map((id) => `<option value="${esc(id)}">${esc(id)}</option>`).join("");
    if (!list.length) html += '<option value="" disabled>(offline)</option>';
    sel.innerHTML = html;
    if (cur) sel.value = cur;
  });
}

async function refreshLmModels() {
  const r = await fetch("/api/lm/models").then((x) => x.json());
  if (r.ok) {
    lmModels = r;
    $("cfg-lm-status").textContent = `${r.chat.length} modelos locais`;
    fillLmSelects();
  } else {
    $("cfg-lm-status").textContent = r.error || "LM offline";
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
    const rt = cfg.runtime || {};
    $("cfg-runtime").textContent = rt.lmUrl
      ? `LM Studio ${rt.lmUrl} · visão ${rt.models?.fast}${rt.models?.deep && rt.models.deep !== rt.models.fast ? ` · insights ${rt.models.deep}` : ""}${rt.models?.embed ? ` · embed ${rt.models.embed}` : ""} · whisper ${rt.models?.whisper} (${rt.whisperDeviceEffective})`
      : "";
    applyConfigToForm(cfg);
    await refreshLmModels();
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
    else if (el.type === "number") v = v === "" ? null : Number(String(v).replace(",", "."));
    else if (v === "") v = null;
    set(patch, el.dataset.path, v);
  });
  delete patch.configPath; delete patch.version; delete patch.runtime;
  await fetch("/api/config", { method: "PUT", headers: { "Content-Type": "application/json" }, body: JSON.stringify(patch) });
  toast("Config salva");
};
$("cfg-reload").onclick = loadConfigForm;
$("cfg-lm-refresh").onclick = refreshLmModels;
$("lb-close").onclick = () => $("lightbox").classList.remove("open");

loadDay();
setInterval(loadDay, 5000);
