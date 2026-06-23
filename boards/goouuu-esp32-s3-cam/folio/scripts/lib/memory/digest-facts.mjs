/** Shared Pass B/C fields for profile_facts and memory_chunks indexing. */

export function profileSlug(category, text) {
  return `${category}:${String(text).slice(0, 48).replace(/\s+/g, "_")}`;
}

export function* iterDigestProfileFacts(passB, passC) {
  for (const p of passB?.patterns ?? []) {
    if (typeof p === "string" && p.length > 4) {
      yield { category: "pattern", text: p, confidence: 0.65, memoryKind: "pattern", weight: 0.85 };
    }
  }

  for (const d of passB?.decisions_real ?? []) {
    const text = d.text ?? d;
    if (typeof text === "string" && text.length > 4) {
      yield {
        category: "decision",
        text,
        confidence: d.confidence ?? 0.75,
        memoryKind: "decision",
        weight: 1.25,
        evidence: d.evidence ?? [],
      };
    }
  }

  for (const loop of passB?.open_loops ?? []) {
    if (typeof loop === "string" && loop.length > 4) {
      yield {
        category: "open",
        text: loop,
        confidence: 0.7,
        memoryKind: "open_loop",
        weight: 1.1,
      };
    }
  }

  for (const c of passC?.approved_claims ?? []) {
    if (c.text?.length > 4) {
      yield {
        category: "claim",
        text: c.text,
        confidence: c.confidence ?? 0.8,
        memoryKind: "claim",
        weight: 1.2,
        evidence: c.evidence ?? [],
      };
    }
  }
}
