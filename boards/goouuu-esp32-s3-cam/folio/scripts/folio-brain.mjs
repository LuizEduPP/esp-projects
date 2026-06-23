#!/usr/bin/env node
/**
 * folio-brain — ingest server, processing loop, timeline UI.
 * ESP32 folio-node pushes audio/frames/events here. No responses to room.
 */
import { createServer } from "node:http";
import { CFG } from "./lib/config.mjs";
import { getDigest, openDb, timelineForDay } from "./lib/db.mjs";
import {
  ingestAudioChunk,
  ingestEvent,
  ingestFrame,
  startProcessingLoop,
} from "./lib/pipeline.mjs";
import { errMsg } from "./lib/util.mjs";

const today = () => new Date().toISOString().slice(0, 10);

async function readBody(req, maxBytes = 512 * 1024) {
  const chunks = [];
  let size = 0;
  for await (const chunk of req) {
    size += chunk.length;
    if (size > maxBytes) {
      throw new Error("body too large");
    }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks);
}

const VIEW_HTML = `<!DOCTYPE html>
<html lang="pt-BR"><head>
<meta charset="utf-8"/><meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Folio</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:Georgia,"Times New Roman",serif;background:#0f1114;color:#e6e2d8;line-height:1.65}
header{padding:14px 20px;border-bottom:1px solid #2a2e36;display:flex;gap:16px;align-items:baseline;flex-wrap:wrap}
header h1{font-size:1.1rem;font-weight:600;letter-spacing:.04em}
header span{color:#8a8680;font-size:.85rem;font-family:system-ui,sans-serif}
nav a{color:#c4a574;margin-right:12px;font-family:system-ui,sans-serif;font-size:.85rem}
main{max-width:820px;margin:0 auto;padding:20px}
.panel{background:#16191f;border:1px solid #2a2e36;border-radius:8px;padding:20px;margin-bottom:16px}
.panel h2{font-family:system-ui,sans-serif;font-size:.7rem;text-transform:uppercase;letter-spacing:.08em;color:#8a8680;margin-bottom:12px}
.prose{font-size:1.05rem;white-space:pre-wrap}
.prose em{color:#8a8680;font-size:.9rem}
.timeline{font-family:system-ui,sans-serif;font-size:.82rem}
.row{padding:8px 0;border-bottom:1px solid #22262e;display:grid;grid-template-columns:72px 1fr;gap:10px}
.row .at{color:#8a8680}
.tag{display:inline-block;padding:1px 6px;border-radius:4px;font-size:.7rem;margin-right:6px}
.tag.utt{background:#1e2a22;color:#9fd4ae}
.tag.frm{background:#1e2430;color:#9eb8e8}
.tag.evt{background:#2a2420;color:#e8c49e}
.err{color:#e8a0a0;font-family:system-ui,sans-serif;font-size:.85rem}
button{font-family:system-ui,sans-serif;background:#2a2418;color:#e6d5b8;border:1px solid #4a4030;border-radius:6px;padding:8px 14px;cursor:pointer}
button:disabled{opacity:.5}
</style></head><body>
<header>
  <h1>Folio</h1>
  <span id="meta">passive day witness</span>
  <nav style="margin-left:auto">
    <a href="#" id="tab-today">Hoje</a>
    <a href="#" id="tab-timeline">Timeline</a>
  </nav>
</header>
<main>
  <div class="panel">
    <h2>Crônica</h2>
    <div id="digest" class="prose">Ainda sem digest para hoje. Rode <code>yarn folio:digest</code> após acumular dados.</div>
    <p style="margin-top:14px"><button id="run-digest">Gerar digest agora</button></p>
    <p id="digest-err" class="err" hidden></p>
  </div>
  <div class="panel">
    <h2>Timeline</h2>
    <div id="timeline" class="timeline"></div>
  </div>
</main>
<script>
const day=new URLSearchParams(location.search).get('day')||new Date().toISOString().slice(0,10);
document.getElementById('meta').textContent='· '+day+' · brain :${CFG.port}';
async function load(){
  const tl=await(await fetch('/api/timeline?day='+day)).json();
  const el=document.getElementById('timeline');
  el.innerHTML=tl.items.map(i=>{
    const t=i.at.slice(11,19);
    if(i.type==='utterance')return '<div class="row"><span class="at">'+t+'</span><span><span class="tag utt">fala</span>'+escapeHtml(i.text)+'</span></div>';
    if(i.type==='frame')return '<div class="row"><span class="at">'+t+'</span><span><span class="tag frm">visão</span>'+escapeHtml(i.caption||'')+'</span></div>';
    return '<div class="row"><span class="at">'+t+'</span><span><span class="tag evt">'+i.kind+'</span></span></div>';
  }).join('')||'<p class="err">Sem eventos ainda.</p>';
  try{
    const d=await(await fetch('/api/digest?day='+day)).json();
    document.getElementById('digest').textContent=d.prose||'(vazio)';
  }catch(e){}
}
function escapeHtml(s){return String(s).replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');}
document.getElementById('run-digest').onclick=async()=>{
  const btn=document.getElementById('run-digest');
  const err=document.getElementById('digest-err');
  btn.disabled=true;err.hidden=true;
  try{
    const r=await fetch('/api/digest/run?day='+day,{method:'POST'});
    const j=await r.json();
    if(!r.ok)throw new Error(j.error||r.status);
    document.getElementById('digest').textContent=j.prose;
  }catch(e){err.hidden=false;err.textContent=e.message}
  finally{btn.disabled=false}
};
load();setInterval(load,8000);
</script></body></html>`;

function startServer() {
  openDb();

  const server = createServer(async (req, res) => {
    const path = req.url?.split("?")[0] ?? "/";
    const qs = new URL(req.url ?? "/", `http://127.0.0.1:${CFG.port}`).searchParams;

    try {
      if (path === "/" || path === "/index.html") {
        res.writeHead(200, { "Content-Type": "text/html; charset=utf-8" });
        res.end(VIEW_HTML);
        return;
      }

      if (path === "/ingest/audio" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"];
        if (!deviceId) {
          res.writeHead(400, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "X-Folio-Device-Id required" }));
          return;
        }
        const body = await readBody(req, 128 * 1024);
        const result = ingestAudioChunk(String(deviceId), body, req.headers["x-folio-meta"]);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, ...result }));
        return;
      }

      if (path === "/ingest/frame" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"];
        if (!deviceId) {
          res.writeHead(400, { "Content-Type": "application/json" });
          res.end(JSON.stringify({ error: "X-Folio-Device-Id required" }));
          return;
        }
        const body = await readBody(req, 400 * 1024);
        const result = ingestFrame(String(deviceId), body, req.headers["x-folio-meta"]);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, ...result }));
        return;
      }

      if (path === "/ingest/event" && req.method === "POST") {
        const deviceId = req.headers["x-folio-device-id"] ?? "unknown";
        const body = JSON.parse((await readBody(req, 16 * 1024)).toString("utf8"));
        const result = ingestEvent(String(deviceId), body);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(result));
        return;
      }

      if (path === "/api/timeline") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ day, items: timelineForDay(db, day) }));
        return;
      }

      if (path === "/api/digest") {
        const day = qs.get("day") ?? today();
        const db = openDb();
        const row = getDigest(db, day);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify(row ?? { day, prose: null }));
        return;
      }

      if (path === "/api/digest/run" && req.method === "POST") {
        const day = qs.get("day") ?? today();
        const { runDigestPipeline } = await import("./lib/passes.mjs");
        const db = openDb();
        const result = await runDigestPipeline(db, day);
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, day: result.day, prose: result.prose }));
        return;
      }

      if (path === "/api/health") {
        res.writeHead(200, { "Content-Type": "application/json" });
        res.end(JSON.stringify({ ok: true, data_dir: CFG.dataDir, port: CFG.port }));
        return;
      }

      res.writeHead(404);
      res.end("not found");
    } catch (err) {
      res.writeHead(500, { "Content-Type": "application/json" });
      res.end(JSON.stringify({ error: errMsg(err) }));
    }
  });

  server.listen(CFG.port, () => {
    console.log(`folio-brain http://127.0.0.1:${CFG.port}`);
    console.log(`data: ${CFG.dataDir}`);
    console.log(`models: fast=${CFG.modelFast} deep=${CFG.modelDeep}`);
  });
}

function main() {
  startServer();
  startProcessingLoop(5000);
}

main();
