#!/usr/bin/env node
/**
 * RC seguidor — ESP32 captura JPEG, LM Studio (vision) localiza pessoa,
 * este script calcula motores e serve painel em http://localhost:8765
 */
import { createServer } from "node:http";

// ── config ────────────────────────────────────────────────────────────────
const CFG = {
  esp: process.env.ESP_URL ?? "http://192.168.1.101",
  lm: process.env.LM_URL ?? "http://127.0.0.1:1234/v1/chat/completions",
  model: process.env.LM_MODEL ?? "mistralai/ministral-3-3b",
  intervalMs: Number(process.env.INTERVAL_MS ?? "800"),
  keepaliveMs: Number(process.env.KEEPALIVE_MS ?? "250"),
  viewPort: Number(process.env.VIEW_PORT ?? "8765"),
  minConfidence: Number(process.env.MIN_CONFIDENCE ?? "0.55"),
  onFrames: Number(process.env.PERSON_ON_FRAMES ?? "1"),
  offFrames: Number(process.env.PERSON_OFF_FRAMES ?? "3"),
  baseSpeed: Number(process.env.BASE_SPEED ?? "255"),
  turnGain: Number(process.env.TURN_GAIN ?? "380"),
  testDrive: process.env.TEST_DRIVE === "1",
  testSpeed: Number(process.env.TEST_SPEED ?? "200"),
};

const PROMPT =
  "RC car camera image. Describe ONLY what you see. " +
  "Reply raw JSON, no markdown: " +
  '{"person":bool,"confidence":0-1,"cx":0-1,"cy":0-1,"note":"max 12 words"}. ' +
  "person=true ONLY for a real human (face/body). " +
  "false for empty room, wall, furniture, photo, doll, shadow. " +
  "cx/cy = person center (0=left/top, 1=right/bottom). If no person: cx=0.5,cy=0.5,confidence=0.";

// ── estado ────────────────────────────────────────────────────────────────
const state = {
  frame: null,
  det: null,
  ms: 0,
  at: "",
  error: null,
  personLocked: false,
  onStreak: 0,
  offStreak: 0,
};

// ── util ──────────────────────────────────────────────────────────────────
const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));

const errMsg = (err) =>
  [err.message, err.cause?.message, err.cause?.code].filter(Boolean).join(" — ");

function parseJson(text) {
  const clean = text.replace(/```(?:json)?/gi, "").replace(/```/g, "").trim();
  for (const m of [...clean.matchAll(/\{[\s\S]*?\}/g)].reverse()) {
    try {
      return JSON.parse(m[0]);
    } catch {
      /* next */
    }
  }
  return null;
}

function computeMotors(cx, cy) {
  const errX = clamp(cx, 0, 1) - 0.5;
  const turn = Math.round(errX * CFG.turnGain);
  const base = CFG.baseSpeed;
  return {
    left: clamp(base - turn, -255, 255),
    right: clamp(base + turn, -255, 255),
  };
}

function analyzeModel(text) {
  const raw = parseJson(text) ?? {};
  const confidence = clamp(Number(raw.confidence) || 0, 0, 1);
  const cx = clamp(Number(raw.cx) ?? 0.5, 0, 1);
  const cy = clamp(Number(raw.cy) ?? 0.5, 0, 1);
  const personRaw = raw.person === true || raw.person === "true";
  const note = String(raw.note ?? raw.description ?? "").slice(0, 80);
  const seen = personRaw && confidence >= CFG.minConfidence;

  if (seen) {
    state.onStreak++;
    state.offStreak = 0;
  } else {
    state.offStreak++;
    state.onStreak = 0;
  }
  if (state.onStreak >= CFG.onFrames) state.personLocked = true;
  if (state.offStreak >= CFG.offFrames) state.personLocked = false;

  const motors = CFG.testDrive
    ? { left: CFG.testSpeed, right: CFG.testSpeed }
    : state.personLocked || seen
      ? computeMotors(cx, cy)
      : { left: 0, right: 0 };

  return {
    personRaw,
    personLocked: state.personLocked,
    confidence,
    cx,
    cy,
    note,
    seen,
    left: motors.left,
    right: motors.right,
    raw: text.trim(),
  };
}

// ── rede ──────────────────────────────────────────────────────────────────
async function espFetch(path, opts = {}) {
  const res = await fetch(`${CFG.esp}${path}`, opts);
  if (!res.ok) throw new Error(`${path} HTTP ${res.status}`);
  return res;
}

async function fetchFrame() {
  const res = await espFetch("/capture", { signal: AbortSignal.timeout(8000) });
  const buf = Buffer.from(await res.arrayBuffer());
  return { buf, b64: buf.toString("base64") };
}

async function askVision(b64) {
  const body = {
    model: CFG.model,
    temperature: 0.05,
    max_tokens: 160,
    messages: [
      {
        role: "user",
        content: [
          { type: "image_url", image_url: { url: `data:image/jpeg;base64,${b64}` } },
          { type: "text", text: PROMPT },
        ],
      },
    ],
  };
  const res = await fetch(CFG.lm, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(120000),
  });
  if (!res.ok) {
    const t = await res.text();
    throw new Error(`LM Studio ${res.status}: ${t.slice(0, 180)}`);
  }
  const json = await res.json();
  const msg = json?.choices?.[0]?.message ?? {};
  return (msg.content || msg.reasoning_content || "").trim();
}

const lastDrive = { left: 0, right: 0 };

async function sendDrive(left, right) {
  lastDrive.left = left;
  lastDrive.right = right;
  await espFetch("/control", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ left, right }),
    signal: AbortSignal.timeout(3000),
  });
}

/** Reenvia ultimo comando durante inferencia LM (~5s) para nao estourar timeout do ESP. */
function startMotorKeeper() {
  setInterval(async () => {
    if (lastDrive.left === 0 && lastDrive.right === 0) return;
    try {
      await espFetch("/control", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(lastDrive),
        signal: AbortSignal.timeout(3000),
      });
    } catch {
      /* ignore */
    }
  }, CFG.keepaliveMs);
}

// ── loop ──────────────────────────────────────────────────────────────────
async function tick() {
  const t0 = Date.now();
  const { buf, b64 } = await fetchFrame();
  const answer = await askVision(b64);
  const det = analyzeModel(answer);
  await sendDrive(det.left, det.right);

  state.frame = buf;
  state.det = det;
  state.ms = Date.now() - t0;
  state.at = new Date().toISOString();
  state.error = null;

  const tag = det.personLocked ? "SEGUINDO" : det.seen ? "DETECTADO" : "livre";
  console.log(
    `[${state.at}] ${state.ms}ms ${tag} conf=${det.confidence.toFixed(2)} ` +
      `cx=${det.cx.toFixed(2)} L=${det.left} R=${det.right} | ${det.note || answer.slice(0, 60)}`,
  );
  return det;
}

// ── painel web ──────────────────────────────────────────────────────────
const VIEW_HTML = `<!DOCTYPE html>
<html lang="pt-BR"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>RC Car — visao</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:system-ui,sans-serif;background:#0d0f14;color:#e8eaed}
header{padding:10px 16px;border-bottom:1px solid #252830;display:flex;gap:12px;align-items:center;flex-wrap:wrap}
.badge{padding:4px 10px;border-radius:999px;font-size:.8rem;font-weight:600}
.badge.follow{background:#1b4332;color:#95d5b2}
.badge.scan{background:#3d3520;color:#f9c74f}
.badge.idle{background:#3d1f1f;color:#f4a3a3}
main{display:grid;grid-template-columns:1fr 300px;gap:12px;padding:12px;max-width:1100px;margin:0 auto}
@media(max-width:760px){main{grid-template-columns:1fr}}
.panel{background:#161920;border:1px solid #252830;border-radius:10px;overflow:hidden}
.panel h2{font-size:.7rem;text-transform:uppercase;letter-spacing:.05em;color:#888;padding:8px 12px;border-bottom:1px solid #252830}
.wrap{position:relative;background:#000}
#cam,#overlay{display:block;width:100%}
#overlay{position:absolute;left:0;top:0;pointer-events:none}
.stats{padding:12px;font-size:.85rem;display:flex;flex-direction:column;gap:8px}
.row{display:flex;justify-content:space-between}
.bar{height:6px;background:#252830;border-radius:3px;margin-top:3px}
.bar i{display:block;height:100%;background:#4c8bf5;border-radius:3px}
.note{color:#9aa0a6;font-style:italic;min-height:2.4em}
pre{font-size:.7rem;background:#0d0f14;padding:8px;border-radius:6px;max-height:100px;overflow:auto;white-space:pre-wrap}
.err{color:#f4a3a3}
</style></head><body>
<header>
  <strong>RC Car</strong>
  <span id="badge" class="badge idle">sem pessoa</span>
  <span id="meta" style="color:#888;font-size:.8rem"></span>
</header>
<main>
  <div class="panel">
    <h2>Camera (QVGA) — cruz = centro detectado</h2>
    <div class="wrap">
      <img id="cam" alt="cam"/>
      <canvas id="overlay"></canvas>
    </div>
  </div>
  <div class="panel">
    <h2>Deteccao</h2>
    <div class="stats">
      <div class="row"><span>Confianca</span><span id="conf">—</span></div>
      <div class="row"><span>Modelo diz person</span><span id="personRaw">—</span></div>
      <div class="row"><span>Confirmado (histerese)</span><span id="locked">—</span></div>
      <div class="row"><span>Centro cx / cy</span><span id="xy">—</span></div>
      <p class="note" id="note">—</p>
      <div><span>Motor E</span><div class="bar"><i id="bL"></i></div></div>
      <div><span>Motor D</span><div class="bar"><i id="bD"></i></div></div>
      <pre id="raw"></pre>
      <p id="err" class="err" hidden></p>
    </div>
  </div>
</main>
<script>
const pct=v=>Math.round(Math.abs(v)/255*100);
function drawOverlay(cx,cy,locked){
  const img=document.getElementById('cam');
  const c=document.getElementById('overlay');
  const w=img.clientWidth,h=img.clientHeight;
  c.width=w;c.height=h;
  const ctx=c.getContext('2d');
  ctx.clearRect(0,0,w,h);
  ctx.strokeStyle=locked?'#95d5b2':'#f9c74f';
  ctx.lineWidth=2;
  const x=cx*w,y=cy*h;
  ctx.beginPath();ctx.moveTo(x-16,y);ctx.lineTo(x+16,y);ctx.moveTo(x,y-16);ctx.lineTo(x,y+16);ctx.stroke();
  ctx.strokeStyle='rgba(255,255,255,.25)';ctx.setLineDash([4,4]);
  ctx.beginPath();ctx.moveTo(w/2,0);ctx.lineTo(w/2,h);ctx.stroke();
  ctx.setLineDash([]);
}
async function poll(){
  try{
    const s=await(await fetch('/api/state')).json();
    const img=document.getElementById('cam');
    img.onload=()=>drawOverlay(s.cx,s.cy,s.personLocked);
    img.src='/frame.jpg?t='+Date.now();
    const b=document.getElementById('badge');
    if(s.personLocked){b.textContent='SEGUINDO pessoa';b.className='badge follow'}
    else if(s.seen){b.textContent='detectando...';b.className='badge scan'}
    else{b.textContent='sem pessoa';b.className='badge idle'}
    document.getElementById('meta').textContent=s.ms+'ms · '+s.model;
    document.getElementById('conf').textContent=(s.confidence*100).toFixed(0)+'%';
    document.getElementById('personRaw').textContent=s.personRaw?'sim':'nao';
    document.getElementById('locked').textContent=s.personLocked?'SIM':'nao';
    document.getElementById('xy').textContent=s.cx.toFixed(2)+' / '+s.cy.toFixed(2);
    document.getElementById('note').textContent=s.note||'(sem descricao)';
    document.getElementById('bL').style.width=pct(s.left)+'%';
    document.getElementById('bD').style.width=pct(s.right)+'%';
    document.getElementById('raw').textContent=s.answer||'';
    document.getElementById('err').hidden=!s.error;
    document.getElementById('err').textContent=s.error||'';
  }catch(e){document.getElementById('err').hidden=false;document.getElementById('err').textContent=e.message}
}
setInterval(poll,400);poll();
</script></body></html>`;

function startViewer() {
  createServer((req, res) => {
    const path = req.url?.split("?")[0] ?? "/";
    if (path === "/" || path === "/index.html") {
      res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
      res.end(VIEW_HTML);
      return;
    }
    if (path === "/frame.jpg") {
      if (!state.frame) {
        res.writeHead(204);
        res.end();
        return;
      }
      res.writeHead(200, { "Content-Type": "image/jpeg", "Cache-Control": "no-store" });
      res.end(state.frame);
      return;
    }
    if (path === "/api/state") {
      const d = state.det ?? {};
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(
        JSON.stringify({
          ...d,
          left: d.left ?? 0,
          right: d.right ?? 0,
          answer: d.raw ?? "",
          ms: state.ms,
          at: state.at,
          error: state.error,
          model: CFG.model,
          esp: CFG.esp,
        }),
      );
      return;
    }
    res.writeHead(404);
    res.end();
  }).listen(CFG.viewPort, () => console.log(`Painel: http://localhost:${CFG.viewPort}`));
}

async function main() {
  console.log(`RC seguidor | ESP=${CFG.esp} | ${CFG.model}`);
  console.log(`conf>=${CFG.minConfidence} histerese ${CFG.onFrames}/${CFG.offFrames} frames`);
  if (CFG.testDrive) {
    console.log(`*** TEST_DRIVE=1 — motores fixos L=${CFG.testSpeed} R=${CFG.testSpeed} (ignora IA) ***`);
  }
  startViewer();
  startMotorKeeper();

  try {
    const st = await (await espFetch("/status", { signal: AbortSignal.timeout(5000) })).json();
    console.log(`ESP32 OK ip=${st.ip}`);
  } catch (err) {
    console.error(`ESP inacessivel: ${errMsg(err)}`);
    process.exit(1);
  }

  for (;;) {
    let det = { left: 0, right: 0 };
    try {
      det = await tick();
    } catch (err) {
      state.error = errMsg(err);
      console.error(`[erro] ${state.error}`);
      try {
        await sendDrive(0, 0);
      } catch {
        /* ignore */
      }
    }
    await new Promise((r) => setTimeout(r, CFG.intervalMs));
  }
}

main();
