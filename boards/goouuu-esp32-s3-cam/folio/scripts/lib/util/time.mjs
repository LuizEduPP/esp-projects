export function dayFromIso(iso) {
  return iso.slice(0, 10);
}

export function today() {
  const d = new Date();
  const y = d.getFullYear();
  const m = String(d.getMonth() + 1).padStart(2, "0");
  const day = String(d.getDate()).padStart(2, "0");
  return `${y}-${m}-${day}`;
}

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
