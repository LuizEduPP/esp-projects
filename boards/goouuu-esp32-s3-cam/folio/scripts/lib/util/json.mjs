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
