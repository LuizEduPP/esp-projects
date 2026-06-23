/** Shared Pass B/C fields for profile_facts and memory_chunks indexing. */

import { CFG } from "../config.mjs";
import { upsertProfileFact } from "../db.mjs";

export const DIGEST_FACT_RULES = {
  pattern: { category: "pattern", confidence: 0.65, memoryKind: "pattern", weight: 0.85 },
  decision: { category: "decision", confidence: 0.75, memoryKind: "decision", weight: 1.25 },
  open_loop: { category: "open", confidence: 0.7, memoryKind: "open_loop", weight: 1.1 },
  claim: { category: "claim", confidence: 0.8, memoryKind: "claim", weight: 1.2 },
};

function factLongEnough(text) {
  return String(text ?? "").trim().length >= CFG.memoryMinFactTextLength;
}

export function profileSlug(category, text) {
  return `${category}:${String(text).slice(0, 48).replace(/\s+/g, "_")}`;
}

export function* iterDigestProfileFacts(passB, passC) {
  for (const p of passB?.patterns ?? []) {
    if (typeof p === "string" && factLongEnough(p)) {
      const rule = DIGEST_FACT_RULES.pattern;
      yield { ...rule, text: p };
    }
  }

  for (const d of passB?.decisions_real ?? []) {
    const text = d.text ?? d;
    if (typeof text === "string" && factLongEnough(text)) {
      const rule = DIGEST_FACT_RULES.decision;
      yield {
        ...rule,
        text,
        confidence: d.confidence ?? rule.confidence,
        evidence: d.evidence ?? [],
      };
    }
  }

  for (const loop of passB?.open_loops ?? []) {
    if (typeof loop === "string" && factLongEnough(loop)) {
      const rule = DIGEST_FACT_RULES.open_loop;
      yield { ...rule, text: loop };
    }
  }

  for (const c of passC?.approved_claims ?? []) {
    if (factLongEnough(c.text)) {
      const rule = DIGEST_FACT_RULES.claim;
      yield {
        ...rule,
        text: c.text,
        confidence: c.confidence ?? rule.confidence,
        evidence: c.evidence ?? [],
      };
    }
  }
}

export function syncDigestProfile(db, day, passB, passC) {
  for (const fact of iterDigestProfileFacts(passB, passC)) {
    upsertProfileFact(db, profileSlug(fact.category, fact.text), fact.text, day, fact.confidence);
  }
}
