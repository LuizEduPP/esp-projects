const $ = (id) => document.getElementById(id);

let day = new URLSearchParams(location.search).get("day");
let serverToday = null;
let filter = "all";
let cfgCache = null;
let lmModels = { chat: [], embed: [], rerank: [], ok: false };
let timelineGroups = [];
let speakerNames = {};
const SPEECH_PREVIEW = 4;

function localTodayIso() {
  const d = new Date();
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const dd = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${dd}`;
}

function isoFromDate(d) {
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const dd = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${dd}`;
}

function effectiveToday() {
  return serverToday || localTodayIso();
}

function dayTitle(d) {
  const fmt = (iso) => {
    try {
      return new Date(`${iso}T12:00:00`).toLocaleDateString("pt-BR", {
        weekday: "long",
        day: "numeric",
        month: "long",
      });
    } catch {
      return iso;
    }
  };
  if (d === effectiveToday()) {
    return `Hoje · ${fmt(d)}`;
  }
  return fmt(d);
}

function humanDayMeta(iso) {
  if (!iso) return "";
  try {
    const diff = Date.now() - new Date(iso).getTime();
    if (diff < 120_000) return "agora há pouco";
    if (diff < 3_600_000) return `há ${Math.round(diff / 60_000)} min`;
    return new Date(iso).toLocaleTimeString("pt-BR", { hour: "2-digit", minute: "2-digit" });
  } catch {
    return time(iso);
  }
}

const PHASE_LABEL = {
  index: "Organizando memória…",
  rag: "Lembrando do passado…",
  insights: "Entendendo o dia…",
  error: "Algo deu errado",
  done: "Pronto",
};

const LOCALE_OPTS = ["pt-BR", "pt-PT", "en-US", "en-GB", "es-ES", "fr-FR", "de-DE"];

const SECTIONS = [
  { title: "Geral", fields: [
    { path: "locale", label: "Idioma", type: "select", options: LOCALE_OPTS },
    { path: "dataDir", label: "Pasta de dados", type: "text", placeholder: "~/.folio" },
  ]},
  { title: "LM Studio", hint: "Modelo carregado no LM Studio — visão + insights", fields: [
    { path: "lm.url", label: "URL", type: "text", placeholder: "http://127.0.0.1:1234/v1" },
    { path: "lm.model", label: "Modelo", type: "lmSelect", pool: "chat" },
    { path: "lm.modelDeep", label: "Modelo insights (opcional)", type: "lmSelect", pool: "chat", emptyOption: "(igual acima)" },
    { path: "lm.modelEmbed", label: "Embeddings (opcional)", type: "lmSelect", pool: "embed", emptyOption: "(desligado)" },
  ]},
  { title: "Captura", fields: [
    { path: "audio.vad.frameMs", label: "Chunk áudio ESP (ms)", type: "number" },
    { path: "frames.captureIntervalMs", label: "Intervalo câmera (ms)", type: "number" },
  ]},
  { title: "Processamento", fields: [
    { path: "pipeline.enabled", label: "Worker ativo", type: "bool" },
    { path: "insights.auto", label: "Insights automáticos", type: "bool" },
    { path: "memory.enabled", label: "Memória RAG", type: "bool" },
    { path: "memory.useEmbeddings", label: "Embeddings no RAG", type: "bool" },
  ]},
  { title: "ESP32", hint: "brainUrl vazio = IP automático na mesma rede", fields: [
    { path: "node.brainUrl", label: "Brain URL", type: "text", placeholder: "http://192.168.x.x:8770" },
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

function parseMomentTime(text) {
  const m = String(text).match(/^(\d{1,2}:\d{2})/);
  return m ? m[1] : null;
}

function scrollToHour(hourStr) {
  const feed = $("witness-feed");
  const mark = [...feed.querySelectorAll(".hour-mark")].find((el) =>
    el.textContent.trim().startsWith(hourStr.slice(0, 2)),
  );
  if (mark) {
    mark.scrollIntoView({ behavior: "smooth", block: "start" });
    return;
  }
  const group = [...feed.querySelectorAll(".witness-group")].find((el) => {
    const header = el.querySelector("header")?.textContent || "";
    return header.includes(hourStr);
  });
  if (group) {
    group.classList.add("highlight");
    group.scrollIntoView({ behavior: "smooth", block: "center" });
    setTimeout(() => group.classList.remove("highlight"), 2000);
  }
}

function setDay(d) {
  day = d;
  $("day-input").value = d;
  $("day-title").textContent = dayTitle(d);
  const u = new URL(location.href);
  u.searchParams.set("day", d);
  history.replaceState(null, "", u);
  loadDay();
}

function shiftDay(n) {
  const d = new Date(`${day}T12:00:00`);
  d.setDate(d.getDate() + n);
  setDay(isoFromDate(d));
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

function filterEntities(entities) {
  return entities.filter((e) => {
    if (e.speaker_id) return true;
    let pat = {};
    try { pat = JSON.parse(e.patterns_json || "{}"); } catch { /* */ }
    if (pat.utterances_today > 0) return true;
    if (e.kind === "pet" && pat.barks_today > 0) return true;
    return false;
  });
}

function renderInsights(ins) {
  const rt = ins.runtime || {};
  const alert = $("insights-alert");
  if (rt.error) {
    alert.hidden = false;
    alert.textContent = `Erro: ${rt.error}`;
    alert.className = "digest-alert error";
  } else if (rt.busy) {
    alert.hidden = false;
    alert.textContent = PHASE_LABEL[rt.phase] || "Pensando…";
    alert.className = "digest-alert";
  } else {
    alert.hidden = true;
  }

  const ragNote = ins.rag_hits != null ? ` · ${ins.rag_hits} memórias` : "";
  $("insights-meta").textContent = ins.updated_at
    ? `${humanDayMeta(ins.updated_at)}${ragNote}`
    : (ins.status?.needed ? "Coletando momentos…" : "Aguardando dados");

  const st = ins.stats || {};
  const speech = st.utterances ?? 0;
  const scenes = st.scenes ?? st.frames ?? 0;
  const summaryEl = $("insights-summary");
  if (ins.summary) {
    summaryEl.textContent = ins.summary;
    summaryEl.classList.remove("muted");
  } else if (speech + scenes > 0) {
    summaryEl.textContent = "Processando fila — resumo em breve…";
    summaryEl.classList.add("muted");
  } else {
    summaryEl.textContent = "Ambiente quieto. Voz ou movimento aparecem aqui.";
    summaryEl.classList.add("muted");
  }

  const moments = ins.moments || [];
  $("insights-moments").innerHTML = moments.length
    ? moments.map((m, i) => {
      const t = parseMomentTime(m);
      const body = t ? m.slice(t.length).replace(/^[\s–—-]+/, "") : m;
      return `<li data-moment="${i}" data-time="${esc(t || "")}">${
        t ? `<span class="moment-time">${esc(t)}</span>` : ""
      }${esc(body || m)}</li>`;
    }).join("")
    : "";

  $("insights-moments").querySelectorAll("li[data-time]").forEach((li) => {
    li.onclick = () => { if (li.dataset.time) scrollToHour(li.dataset.time); };
  });

  const people = filterEntities(ins.entities || []);
  $("insights-people").innerHTML = people.length
    ? people.map((e) => {
      let pat = {};
      try { pat = JSON.parse(e.patterns_json || "{}"); } catch { /* */ }
      const detail = pat.utterances_today
        ? `${pat.utterances_today} falas`
        : pat.barks_today
          ? `${pat.barks_today} latidos`
          : "";
      return `<div class="person"><strong>${esc(e.display_name)}</strong>${
        detail ? ` <span>· ${esc(detail)}</span>` : ""
      }</div>`;
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

  $("witness-feed").innerHTML = list.map((g, gi) => {
    const hourMark = g.showHour ? `<div class="hour-mark">${esc(g.hour_label)}</div>` : "";

    if (g.type === "speech") {
      const who = g.speaker_id ? (speakerNames[g.speaker_id] || g.speaker_id) : null;
      const preview = g.lines.slice(0, SPEECH_PREVIEW);
      const rest = g.lines.length - preview.length;
      const lines = preview.map((ln) => {
        const aud = ln.has_pcm
          ? `<audio controls preload="none" src="/api/audio/${ln.chunk_id}"></audio>`
          : "";
        return `<p class="speech-line">${aud}${esc(ln.text)}</p>`;
      }).join("");
      const moreBtn = rest > 0
        ? `<button type="button" class="speech-more" data-group="${gi}">+ ${rest} falas</button>`
        : "";
      return `${hourMark}<article class="witness-group speech" data-at="${esc(g.at)}">
        <header>${who ? esc(who) : "Fala"} · ${time(g.at)}</header>
        ${lines}${moreBtn}
      </article>`;
    }

    if (g.type === "sound") {
      const repeat = g.count > 1 ? `<span class="repeat-note"> · ${g.count}×</span>` : "";
      const cid = g.chunk_ids?.[g.chunk_ids.length - 1];
      const aud = cid ? `<audio controls preload="none" src="/api/audio/${cid}"></audio>` : "";
      return `${hourMark}<article class="witness-group sound">
        <header><span class="badge sound">${esc(g.sound_label || "Som")}</span> · ${time(g.at)}${repeat}</header>
        ${aud}
      </article>`;
    }

    if (g.type === "scene") {
      const fid = g.frame_ids[g.frame_ids.length - 1];
      const repeat = g.count > 1 ? `<span class="repeat-note"> · ${g.count}×</span>` : "";
      const scene = g.scene_json ? (() => { try { return JSON.parse(g.scene_json); } catch { return null; } })() : null;
      const motion = scene?.motion_level === "high" ? `<span class="badge motion">movimento</span> ` : "";
      return `${hourMark}<article class="witness-group scene">
        <header>${motion}Cena · ${time(g.at)}${repeat}</header>
        <img src="/api/frame/${fid}" alt="" loading="lazy"/>
        <p>${esc(g.caption)}</p>
      </article>`;
    }

    if (filter !== "frames") return "";
    const fid = g.frame_ids?.[g.frame_ids.length - 1];
    const n = g.count ?? g.frame_ids?.length ?? 1;
    return `${hourMark}<article class="witness-group pending">
      <header>${n > 1 ? `${n} na fila…` : "Processando…"}</header>
      ${fid ? `<img src="/api/frame/${fid}" alt="" loading="lazy" class="dim"/>` : ""}
    </article>`;
  }).filter(Boolean).join("");

  $("witness-feed").querySelectorAll("img:not(.dim)").forEach((img) => {
    img.onclick = () => { $("lb-img").src = img.src; $("lightbox").classList.add("open"); };
  });

  $("witness-feed").querySelectorAll(".speech-more").forEach((btn) => {
    btn.onclick = () => {
      const g = list[Number(btn.dataset.group)];
      if (!g) return;
      const article = btn.closest(".witness-group");
      const extra = g.lines.slice(SPEECH_PREVIEW).map((ln) => {
        const aud = ln.has_pcm
          ? `<audio controls preload="none" src="/api/audio/${ln.chunk_id}"></audio>`
          : "";
        return `<p class="speech-line">${aud}${esc(ln.text)}</p>`;
      }).join("");
      btn.insertAdjacentHTML("beforebegin", extra);
      btn.remove();
    };
  });
}

async function loadStatus(ins = {}) {
  const q = await fetch("/api/queue").then((r) => r.json());
  const pa = q.pending?.audio || 0;
  const pf = q.pending?.frames || 0;
  const rt = ins.runtime || {};
  const live = rt.busy ? `<span class="chip live">${PHASE_LABEL[rt.phase] || "…"}</span>` : "";
  const queue = pa + pf
    ? `<span class="chip warn" title="${pa} áudio · ${pf} frames">${pa + pf} na fila</span>`
    : `<span class="chip ok">em dia</span>`;
  $("status-chips").innerHTML = live + queue;
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
  $("witness-stats").textContent = timelineGroups.length
    ? `${timelineGroups.length} momentos · ${sceneCount} cenas · ${speechCount} falas`
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
    ["Pipeline", h.pipeline ? "ativo" : "pausado"],
    ["Whisper", h.stt?.ready ? `${h.stt.backend} · ${h.stt.model}` : "auto (off)"],
    ["Fila", `${h.pending?.audio ?? 0} áudio · ${h.pending?.frames ?? 0} frames`],
    ["Memória", `${h.memory_chunks ?? 0} chunks`],
    ["Insights", h.insights ? "auto" : "manual"],
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
        const opts = f.options.map((o) => `<option value="${o}">${o}</option>`).join("");
        return `<div class="field"><label for="${id}">${f.label}</label><select id="${id}" data-path="${f.path}">${opts}</select></div>`;
      }
      const ph = f.placeholder ? ` placeholder="${esc(f.placeholder)}"` : "";
      return `<div class="field"><label for="${id}">${f.label}</label>
        <input id="${id}" data-path="${f.path}" type="${f.type || "text"}"${ph}/></div>`;
    }).join("");
    return `<section class="cfg-section"><h3>${sec.title}</h3>${hint}${fields}</section>`;
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
    $("cfg-lm-status").textContent = `${r.chat.length} modelos`;
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
      ? `LM ${rt.lmUrl} · ${rt.models?.fast}`
      : "";
    applyConfigToForm(cfg);
    await refreshLmModels();
    applyConfigToForm(cfg);
  });
}

async function initApp() {
  try {
    const h = await fetch("/api/health").then((r) => r.json());
    serverToday = h.today || null;
  } catch { /* offline */ }

  const utcToday = new Date().toISOString().slice(0, 10);
  if (!day) {
    day = effectiveToday();
  } else if (serverToday && day === utcToday && day !== serverToday) {
    // URL was saved with UTC date (off-by-one in Brazil etc.)
    day = serverToday;
  }

  const u = new URL(location.href);
  u.searchParams.set("day", day);
  history.replaceState(null, "", u);

  $("day-input").value = day;
  $("day-title").textContent = dayTitle(day);
  loadDay();
  setInterval(loadDay, 5000);
}

$("day-prev").onclick = () => shiftDay(-1);
$("day-next").onclick = () => shiftDay(1);
$("day-input").onchange = (e) => setDay(e.target.value);

document.querySelectorAll(".filter").forEach((btn) => {
  btn.onclick = () => {
    document.querySelectorAll(".filter").forEach((b) => b.classList.remove("on"));
    btn.classList.add("on");
    filter = btn.dataset.f;
    renderWitnessGroups(timelineGroups);
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

$("btn-insights").onclick = async () => {
  $("btn-insights").disabled = true;
  toast("Gerando resumo…");
  try {
    await fetch(`/api/insights/run?day=${day}`, { method: "POST" });
    loadDay();
    toast("Resumo atualizado");
  } catch (e) { toast(e.message); }
  $("btn-insights").disabled = false;
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

initApp();
