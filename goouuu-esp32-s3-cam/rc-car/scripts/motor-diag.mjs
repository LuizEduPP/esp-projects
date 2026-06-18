#!/usr/bin/env node
/**
 * Bateria de testes HTTP + logs para diagnosticar motores vs fiacao.
 * Uso: ESP_URL=http://192.168.1.101 yarn motor:diag
 */
import { writeFileSync } from "node:fs";

const ESP = process.env.ESP_URL ?? "http://192.168.1.101";
const LOG_FILE = process.env.LOG_FILE ?? "motor-diag.log";

const lines = [];
const log = (msg) => {
  const line = `[${new Date().toISOString()}] ${msg}`;
  console.log(line);
  lines.push(line);
};

const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

async function get(path) {
  const t0 = Date.now();
  const res = await fetch(`${ESP}${path}`, { signal: AbortSignal.timeout(10000) });
  const text = await res.text();
  log(`GET ${path} -> ${res.status} ${Date.now() - t0}ms`);
  return { status: res.status, text, json: tryJson(text) };
}

async function post(path, body) {
  const t0 = Date.now();
  const res = await fetch(`${ESP}${path}`, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body),
    signal: AbortSignal.timeout(60000),
  });
  const text = await res.text();
  log(`POST ${path} ${JSON.stringify(body)} -> ${res.status} ${Date.now() - t0}ms`);
  return { status: res.status, text, json: tryJson(text) };
}

function tryJson(text) {
  try {
    return JSON.parse(text);
  } catch {
    return null;
  }
}

function logMotor(j, label) {
  if (!j?.motor && !j?.pins) {
    log(`  ${label}: (sem dados motor)`);
    return;
  }
  const m = j.motor ?? j;
  log(`  ${label}: running=${m.running} L=${m.left} R=${m.right} cmdAge=${m.cmdAgeMs}ms`);
  if (m.pins) {
    for (const p of m.pins) {
      log(`    ${p.name} GPIO${p.gpio} level=${p.level} (1=HIGH/ativo)`);
    }
  }
}

function analyzePins(motor, step) {
  if (!motor?.pins) return;
  const active = motor.left !== 0 || motor.right !== 0;
  if (!active) return;

  const high = motor.pins.filter((p) => p.level === 1);
  log(`  [analise ${step}] pinos HIGH: ${high.map((p) => `${p.name}(${p.gpio})`).join(", ") || "NENHUM"}`);
  if (high.length === 0) {
    log(`  [analise ${step}] *** PROBLEMA SOFTWARE: comando motor mas nenhum GPIO HIGH ***`);
  } else {
    log(`  [analise ${step}] ESP enviou sinal — se motor nao gira, suspeite fiacao/alimentacao L9110S`);
  }
}

async function run() {
  log(`=== DIAG RC CAR === ESP=${ESP}`);

  // 1. Conectividade
  let r = await get("/status");
  if (r.status !== 200) {
    log("FALHOU: ESP inacessivel");
    saveLog();
    process.exit(1);
  }
  logMotor(r.json, "status inicial");

  r = await get("/diag");
  logMotor(r.json?.motor ?? r.json, "diag");

  // 2. Para tudo
  r = await post("/control", { left: 0, right: 0 });
  logMotor(r.json?.motor, "stop");
  await sleep(500);

  // 3. Frente
  log("--- TESTE frente L=200 R=200 (2s) ---");
  r = await post("/control", { left: 200, right: 200 });
  logMotor(r.json?.motor, "frente cmd");
  analyzePins(r.json?.motor, "frente");
  await sleep(2000);
  r = await get("/diag");
  logMotor(r.json?.motor, "frente apos 2s");
  analyzePins(r.json?.motor, "frente+2s");

  // 4. Stop
  await post("/control", { left: 0, right: 0 });
  await sleep(500);

  // 5. Girar esquerda
  log("--- TESTE gira esq L=200 R=0 ---");
  r = await post("/control", { left: 200, right: 0 });
  analyzePins(r.json?.motor, "gira_esq");
  await sleep(1500);
  await post("/control", { left: 0, right: 0 });
  await sleep(500);

  // 6. Girar direita
  log("--- TESTE gira dir L=0 R=200 ---");
  r = await post("/control", { left: 0, right: 200 });
  analyzePins(r.json?.motor, "gira_dir");
  await sleep(1500);
  await post("/control", { left: 0, right: 0 });
  await sleep(500);

  // 7. Tras
  log("--- TESTE tras L=-200 R=-200 ---");
  r = await post("/control", { left: -200, right: -200 });
  analyzePins(r.json?.motor, "tras");
  await sleep(1500);
  await post("/control", { left: 0, right: 0 });
  await sleep(500);

  // 8. Capture + motor imediato (simula IA)
  log("--- TESTE capture + motor (simula loop IA) ---");
  const cap = await get("/capture");
  log(`  capture: ${cap.status} bytes=${cap.text.length}`);
  r = await post("/control", { left: 180, right: 180 });
  analyzePins(r.json?.motor, "pos_capture");
  await sleep(2000);
  await post("/control", { left: 0, right: 0 });

  // 9. Teste longo no ESP
  log("--- TESTE /test bateria no ESP (~12s) — observe o carro ---");
  r = await post("/test", {});
  logMotor(r.json?.motor, "pos /test");
  if (r.json?.motor?.pins) {
    analyzePins(r.json.motor, "pos_test");
  }

  log("=== FIM ===");
  log("Interpretacao:");
  log("  - GPIO level=1 no serial/diag mas motor parado -> fiacao ou L9110S ou pilhas");
  log("  - GPIO level=0 com L/R != 0 -> bug software (reporte)");
  log(`Log salvo: ${LOG_FILE}`);
  saveLog();
}

function saveLog() {
  writeFileSync(LOG_FILE, lines.join("\n") + "\n");
}

run().catch((e) => {
  log(`ERRO FATAL: ${e.message}`);
  saveLog();
  process.exit(1);
});
