import { upsertProfileFact } from "../db.mjs";

function slugKey(prefix, text) {
  return `${prefix}:${String(text).slice(0, 48).replace(/\s+/g, "_")}`;
}

export function syncProfileFromDigest(db, day, passB, passC) {
  for (const p of passB?.patterns ?? []) {
    if (typeof p === "string" && p.length > 4) {
      upsertProfileFact(db, slugKey("pattern", p), p, day, 0.65);
    }
  }

  for (const d of passB?.decisions_real ?? []) {
    const text = d.text ?? d;
    if (typeof text === "string" && text.length > 4) {
      upsertProfileFact(db, slugKey("decision", text), text, day, d.confidence ?? 0.75);
    }
  }

  for (const loop of passB?.open_loops ?? []) {
    if (typeof loop === "string" && loop.length > 4) {
      upsertProfileFact(db, slugKey("open", loop), loop, day, 0.7);
    }
  }

  for (const c of passC?.approved_claims ?? []) {
    if (c.text?.length > 4) {
      upsertProfileFact(db, slugKey("claim", c.text), c.text, day, c.confidence ?? 0.8);
    }
  }
}
