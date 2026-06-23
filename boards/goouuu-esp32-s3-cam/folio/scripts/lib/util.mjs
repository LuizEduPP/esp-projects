import { mkdirSync, writeFileSync } from "node:fs";
import { dirname } from "node:path";

const NUM_CHANNELS = 1;
const BITS_PER_SAMPLE = 16;

export function pcmToWav(pcmBuffer, sampleRate) {
  const dataSize = pcmBuffer.length;
  const header = Buffer.alloc(44);
  header.write("RIFF", 0);
  header.writeUInt32LE(36 + dataSize, 4);
  header.write("WAVE", 8);
  header.write("fmt ", 12);
  header.writeUInt32LE(16, 16);
  header.writeUInt16LE(1, 20);
  header.writeUInt16LE(NUM_CHANNELS, 22);
  header.writeUInt32LE(sampleRate, 24);
  header.writeUInt32LE((sampleRate * NUM_CHANNELS * BITS_PER_SAMPLE) / 8, 28);
  header.writeUInt16LE((NUM_CHANNELS * BITS_PER_SAMPLE) / 8, 32);
  header.writeUInt16LE(BITS_PER_SAMPLE, 34);
  header.write("data", 36);
  header.writeUInt32LE(dataSize, 40);
  return Buffer.concat([header, pcmBuffer]);
}

export function writeWav(path, pcmBuffer, sampleRate) {
  mkdirSync(dirname(path), { recursive: true });
  writeFileSync(path, pcmToWav(pcmBuffer, sampleRate));
}

export function parseMetaHeader(raw) {
  const out = {};
  if (!raw) {
    return out;
  }
  for (const part of raw.split(";")) {
    const [k, v] = part.split("=");
    if (k && v !== undefined) {
      out[k.trim()] = v.trim();
    }
  }
  return out;
}

export function dayFromIso(iso) {
  return iso.slice(0, 10);
}

/** Local calendar day (YYYY-MM-DD). */
export function today() {
  const d = new Date();
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const day = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${day}`;
}

/** Local calendar day window for SQLite range queries. */
export function dayBounds(day) {
  const start = new Date(`${day}T00:00:00`);
  const end = new Date(`${day}T23:59:59.999`);
  return { start: start.toISOString(), end: end.toISOString() };
}

export function retentionCutoffIso(days) {
  const d = new Date();
  d.setDate(d.getDate() - days);
  return d.toISOString();
}

export function priorDay(day) {
  return dayOffset(day, -1);
}

export function dayOffset(day, days) {
  const d = new Date(`${day}T12:00:00.000Z`);
  d.setUTCDate(d.getUTCDate() + days);
  return d.toISOString().slice(0, 10);
}

export function isoNow() {
  return new Date().toISOString();
}

export function sendJson(res, status, body) {
  res.writeHead(status, { "Content-Type": "application/json" });
  res.end(JSON.stringify(body));
}

export function sendBytes(res, status, body, contentType, cache = "private, max-age=3600") {
  res.writeHead(status, { "Content-Type": contentType, "Cache-Control": cache });
  res.end(body);
}

export function parseJsonLoose(text) {
  const clean = String(text).replace(/```(?:json)?/gi, "").replace(/```/g, "").trim();
  for (const m of [...clean.matchAll(/\{[\s\S]*\}/g)].reverse()) {
    try {
      return JSON.parse(m[0]);
    } catch {
      /* next */
    }
  }
  for (const m of [...clean.matchAll(/\[[\s\S]*\]/g)].reverse()) {
    try {
      return JSON.parse(m[0]);
    } catch {
      /* next */
    }
  }
  return null;
}

export function errMsg(err) {
  return [err?.message, err?.cause?.message, err?.cause?.code].filter(Boolean).join(" — ");
}
