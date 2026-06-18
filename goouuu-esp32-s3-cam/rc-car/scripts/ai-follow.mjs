#!/usr/bin/env node

import { createServer } from "node:http";

const ESP_URL = process.env.ESP_URL ?? "http://192.168.1.101";
const LM_URL = process.env.LM_URL ?? "http://127.0.0.1:1234/v1/chat/completions";
const LM_MODEL = process.env.LM_MODEL ?? "mistralai/ministral-3-3b";
const INTERVAL_MS = Number(process.env.INTERVAL_MS ?? "800");
const KEEPALIVE_MS = Number(process.env.KEEPALIVE_MS ?? "300");
const VIEW_PORT = Number(process.env.VIEW_PORT ?? "8765");

/** @type {{ frame: Buffer|null, drive: object, answer: string, ms: number, at: string, error: string|null }} */
const viewState = {
  frame: null,
  drive: { left: 0, right: 0, person: false, modelLeft: 0, modelRight: 0 },
  answer: "",
  ms: 0,
  at: "",
  error: null,
};

const PROMPT =
  "Photo from RC car camera. Is a HUMAN person visible in this image? " +
  "Reply with raw JSON only (no markdown, no code fence): " +
  '{"left":number,"right":number,"person":boolean}. ' +
  "Motors -255..255, positive=forward. " +
  "If NO human: must be exactly {\"left\":0,\"right\":0,\"person\":false}. " +
  "If human visible: person must be true. " +
  "Steer to center the person: person on left of frame → left<right; on right → left>right; centered → left≈right forward ~140.";

function stripMarkdown(text) {
  return text
    .replace(/```(?:json)?\s*/gi, "")
    .replace(/```/g, "")
    .trim();
}

function parsePerson(value) {
  return value === true || value === 1 || value === "true" || value === "yes";
}

function parseDrive(text) {
  const clean = stripMarkdown(text);
  const matches = [...clean.matchAll(/\{[\s\S]*?\}/g)];
  for (let i = matches.length - 1; i >= 0; i--) {
    try {
      const data = JSON.parse(matches[i][0]);
      if (!("left" in data || "right" in data || "person" in data)) {
        continue;
      }
      const person = parsePerson(data.person);
      const left = clamp(Number(data.left) || 0, -255, 255);
      const right = clamp(Number(data.right) || 0, -255, 255);
      return {
        left: person ? left : 0,
        right: person ? right : 0,
        person,
        modelLeft: left,
        modelRight: right,
        raw: clean,
      };
    } catch {
      /* try previous match */
    }
  }
  return { left: 0, right: 0, person: false, modelLeft: 0, modelRight: 0, raw: clean };
}

function clamp(v, min, max) {
  return Math.max(min, Math.min(max, v));
}

function errMsg(err) {
  const parts = [err.message];
  if (err.cause?.message) {
    parts.push(err.cause.message);
  }
  if (err.cause?.code) {
    parts.push(err.cause.code);
  }
  return parts.join(" — ");
}

async function probeEsp() {
  const res = await fetch(`${ESP_URL}/status`, { signal: AbortSignal.timeout(5000) });
  if (!res.ok) {
    throw new Error(`status HTTP ${res.status}`);
  }
  const json = await res.json();
  console.log(`ESP32 OK — ip=${json.ip ?? "?"}`);
}

async function fetchFrame() {
  let res;
  try {
    res = await fetch(`${ESP_URL}/capture`, { signal: AbortSignal.timeout(8000) });
  } catch (err) {
    throw new Error(`capture ${ESP_URL}: ${errMsg(err)}`, { cause: err });
  }
  if (!res.ok) {
    throw new Error(`capture HTTP ${res.status}`);
  }
  const buf = Buffer.from(await res.arrayBuffer());
  return { buf, b64: buf.toString("base64") };
}

async function askLmStudio(imageB64) {
  const body = {
    model: LM_MODEL,
    temperature: 0.1,
    max_tokens: 128,
    messages: [
      {
        role: "user",
        content: [
          {
            type: "image_url",
            image_url: { url: `data:image/jpeg;base64,${imageB64}` },
          },
          { type: "text", text: PROMPT },
        ],
      },
    ],
  };

  let res;
  try {
    res = await fetch(LM_URL, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
      signal: AbortSignal.timeout(120000),
    });
  } catch (err) {
    throw new Error(`LM Studio ${LM_URL}: ${errMsg(err)}`, { cause: err });
  }

  if (!res.ok) {
    const errText = await res.text();
    throw new Error(`LM Studio HTTP ${res.status}: ${errText.slice(0, 200)}`);
  }

  const json = await res.json();
  const msg = json?.choices?.[0]?.message ?? {};
  const content = typeof msg.content === "string" ? msg.content.trim() : "";
  const reasoning =
    typeof msg.reasoning_content === "string" ? msg.reasoning_content.trim() : "";

  if (!content && reasoning) {
    console.warn("[aviso] modelo respondeu so reasoning — verifique LM Studio");
  }

  return content || reasoning || JSON.stringify(msg);
}

async function sendDrive(left, right) {
  let res;
  try {
    res = await fetch(`${ESP_URL}/control`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ left, right }),
      signal: AbortSignal.timeout(3000),
    });
  } catch (err) {
    throw new Error(`control ${ESP_URL}: ${errMsg(err)}`, { cause: err });
  }
  if (!res.ok) {
    throw new Error(`control HTTP ${res.status}`);
  }
}

async function keepalive(left, right, totalMs) {
  if (left === 0 && right === 0) {
    await new Promise((r) => setTimeout(r, totalMs));
    return;
  }
  const end = Date.now() + totalMs;
  while (Date.now() < end) {
    await new Promise((r) => setTimeout(r, Math.min(KEEPALIVE_MS, end - Date.now())));
    try {
      await sendDrive(left, right);
    } catch {
      /* ignore */
    }
  }
}

async function tick() {
  const t0 = Date.now();
  const { buf, b64 } = await fetchFrame();
  const answer = await askLmStudio(b64);
  const drive = parseDrive(answer);
  await sendDrive(drive.left, drive.right);
  const ms = Date.now() - t0;

  viewState.frame = buf;
  viewState.drive = drive;
  viewState.answer = answer;
  viewState.ms = ms;
  viewState.at = new Date().toISOString();
  viewState.error = null;

  const applied = `L=${drive.left} R=${drive.right}`;
  const model =
    drive.person || (drive.modelLeft === 0 && drive.modelRight === 0)
      ? ""
      : ` (modelo queria L=${drive.modelLeft} R=${drive.modelRight})`;
  console.log(
    `[${new Date().toISOString()}] ${ms}ms person=${drive.person} ${applied}${model}`,
  );
  if (!drive.person && (drive.modelLeft !== 0 || drive.modelRight !== 0)) {
    console.warn("[aviso] modelo mandou motores com person=false — ignorado, carro parado");
  }
  if (!drive.raw) {
    console.warn("[aviso] resposta vazia — verifique LM Studio e modelo vision");
  }
  return drive;
}

const VIEW_HTML = `<!DOCTYPE html>
<html lang="pt-BR">
<head>
  <meta charset="utf-8"/>
  <meta name="viewport" content="width=device-width,initial-scale=1"/>
  <title>RC Car — visao IA</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:system-ui,sans-serif;background:#0f1117;color:#e8eaed;min-height:100vh}
    header{padding:12px 20px;border-bottom:1px solid #2a2f3a;display:flex;gap:16px;align-items:center;flex-wrap:wrap}
    h1{font-size:1.1rem;font-weight:600}
    .badge{padding:4px 12px;border-radius:999px;font-size:.85rem;font-weight:600}
    .badge.ok{background:#1b4332;color:#95d5b2}
    .badge.no{background:#3d1f1f;color:#f4a3a3}
    main{display:grid;grid-template-columns:1fr 320px;gap:16px;padding:16px;max-width:1200px;margin:0 auto}
    @media(max-width:800px){main{grid-template-columns:1fr}}
    .panel{background:#1a1d27;border:1px solid #2a2f3a;border-radius:12px;overflow:hidden}
    .panel h2{font-size:.75rem;text-transform:uppercase;letter-spacing:.06em;color:#9aa0a6;padding:10px 14px;border-bottom:1px solid #2a2f3a}
    #cam{display:block;width:100%;background:#000;min-height:200px}
    .stats{padding:14px;display:flex;flex-direction:column;gap:12px}
    .row{display:flex;justify-content:space-between;font-size:.9rem}
    .bar{height:8px;background:#2a2f3a;border-radius:4px;overflow:hidden;margin-top:4px}
    .bar>i{display:block;height:100%;background:#4c8bf5;border-radius:4px}
    .bar.neg>i{background:#e07a5f;margin-left:auto}
    pre{font-size:.75rem;background:#12141c;padding:12px;border-radius:8px;overflow:auto;max-height:160px;white-space:pre-wrap;word-break:break-word}
    .err{color:#f4a3a3;font-size:.85rem;padding:14px}
  </style>
</head>
<body>
  <header>
    <h1>RC Car — visao IA</h1>
    <span id="personBadge" class="badge no">sem pessoa</span>
    <span id="latency" style="color:#9aa0a6;font-size:.85rem"></span>
  </header>
  <main>
    <div class="panel">
      <h2>Camera ESP32</h2>
      <img id="cam" alt="camera"/>
    </div>
    <div class="panel">
      <h2>Deteccao</h2>
      <div class="stats">
        <div>
          <div class="row"><span>Motor esquerdo</span><span id="vL">0</span></div>
          <div class="bar"><i id="bL" style="width:0%"></i></div>
        </div>
        <div>
          <div class="row"><span>Motor direito</span><span id="vR">0</span></div>
          <div class="bar"><i id="bR" style="width:0%"></i></div>
        </div>
        <div class="row"><span>Modelo quer L/R</span><span id="modelLR">—</span></div>
        <div class="row"><span>Atualizado</span><span id="at">—</span></div>
        <pre id="raw">aguardando...</pre>
        <p id="err" class="err" hidden></p>
      </div>
    </div>
  </main>
  <script>
    const pct = (v) => Math.round(Math.abs(v) / 255 * 100);
    async function poll() {
      try {
        const r = await fetch('/api/state');
        const s = await r.json();
        document.getElementById('cam').src = '/frame.jpg?t=' + Date.now();
        const b = document.getElementById('personBadge');
        b.textContent = s.person ? 'PESSOA detectada' : 'sem pessoa';
        b.className = 'badge ' + (s.person ? 'ok' : 'no');
        document.getElementById('latency').textContent = s.ms + ' ms · ' + s.model;
        document.getElementById('vL').textContent = s.left;
        document.getElementById('vR').textContent = s.right;
        document.getElementById('bL').style.width = pct(s.left) + '%';
        document.getElementById('bR').style.width = pct(s.right) + '%';
        document.getElementById('modelLR').textContent = s.modelLeft + ' / ' + s.modelRight;
        document.getElementById('at').textContent = s.at ? new Date(s.at).toLocaleTimeString() : '—';
        document.getElementById('raw').textContent = s.answer || '(vazio)';
        const e = document.getElementById('err');
        if (s.error) { e.hidden = false; e.textContent = s.error; }
        else { e.hidden = true; }
      } catch (err) {
        document.getElementById('err').hidden = false;
        document.getElementById('err').textContent = err.message;
      }
    }
    setInterval(poll, 400);
    poll();
  </script>
</body>
</html>`;

function startViewer() {
  const server = createServer((req, res) => {
    const url = req.url?.split("?")[0] ?? "/";
    if (url === "/" || url === "/index.html") {
      res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
      res.end(VIEW_HTML);
      return;
    }
    if (url === "/frame.jpg") {
      if (!viewState.frame) {
        res.writeHead(204);
        res.end();
        return;
      }
      res.writeHead(200, { "Content-Type": "image/jpeg", "Cache-Control": "no-store" });
      res.end(viewState.frame);
      return;
    }
    if (url === "/api/state") {
      const d = viewState.drive;
      res.writeHead(200, { "Content-Type": "application/json" });
      res.end(
        JSON.stringify({
          person: d.person,
          left: d.left,
          right: d.right,
          modelLeft: d.modelLeft,
          modelRight: d.modelRight,
          answer: viewState.answer,
          ms: viewState.ms,
          at: viewState.at,
          error: viewState.error,
          model: LM_MODEL,
          esp: ESP_URL,
        }),
      );
      return;
    }
    res.writeHead(404);
    res.end("not found");
  });
  server.listen(VIEW_PORT, () => {
    console.log(`Painel: http://localhost:${VIEW_PORT}`);
  });
}

async function main() {
  console.log("RC seguidor IA — LM Studio");
  console.log(`ESP=${ESP_URL} LM=${LM_URL} model=${LM_MODEL} interval=${INTERVAL_MS}ms`);
  console.log("Ctrl+C para parar (carro para sozinho apos ~4s sem comando)");
  startViewer();

  try {
    await probeEsp();
  } catch (err) {
    console.error(`[erro] ESP inacessivel: ${errMsg(err)}`);
    console.error(`Defina ESP_URL=http://IP_DO_ESP32 yarn rc-car:ai`);
    process.exit(1);
  }

  for (;;) {
    let drive = { left: 0, right: 0 };
    try {
      drive = await tick();
    } catch (err) {
      viewState.error = errMsg(err);
      console.error(`[erro] ${errMsg(err)}`);
      try {
        await sendDrive(0, 0);
      } catch {
        /* ignore */
      }
    }
    await keepalive(drive.left, drive.right, INTERVAL_MS);
  }
}

main();
