/** Natural caption + scene fingerprint for dedup. */

export function formatSceneCaption(scene) {
  if (!scene || typeof scene !== "object") {
    return "";
  }
  if (scene.summary && String(scene.summary).trim()) {
    return String(scene.summary).trim();
  }
  if (scene.unchanged) {
    return "";
  }
  const activity = scene.activity?.trim();
  const sceneLabel = scene.scene?.trim();
  const note = scene.note?.trim();
  if (scene.person_present && activity) {
    const who = Number(scene.people) > 1 ? `${scene.people} pessoas` : "Alguém";
    return note ? `${who} ${activity.toLowerCase()} — ${note}` : `${who} ${activity.toLowerCase()}.`;
  }
  if (activity && activity !== "unknown" && !/^nothing|empty|nada|vazio/i.test(activity)) {
    return note ? `${activity}. ${note}` : `${activity}.`;
  }
  if (sceneLabel && sceneLabel !== "unknown") {
    return note ? `${sceneLabel}. ${note}` : `${sceneLabel}.`;
  }
  return note || "Ambiente quieto.";
}

export function sceneFingerprint(scene) {
  if (!scene || scene.unchanged) {
    return "unchanged";
  }
  const key = [
    scene.person_present ? "p" : "-",
    scene.people ?? 0,
    norm(scene.scene),
    norm(scene.activity),
    scene.mood ?? "",
  ].join("|");
  return key;
}

function norm(s) {
  return String(s ?? "")
    .toLowerCase()
    .normalize("NFD")
    .replace(/\p{M}/gu, "")
    .replace(/[^\p{L}\p{N}\s]/gu, " ")
    .replace(/\s+/g, " ")
    .trim()
    .slice(0, 48);
}

export function captionsSimilar(a, b) {
  const na = norm(a);
  const nb = norm(b);
  if (!na || !nb) {
    return false;
  }
  if (na === nb) {
    return true;
  }
  if (na.includes(nb) || nb.includes(na)) {
    return true;
  }
  const wa = new Set(na.split(" ").filter((w) => w.length > 3));
  const wb = new Set(nb.split(" ").filter((w) => w.length > 3));
  if (!wa.size || !wb.size) {
    return false;
  }
  let overlap = 0;
  for (const w of wa) {
    if (wb.has(w)) {
      overlap++;
    }
  }
  return overlap / Math.min(wa.size, wb.size) >= 0.65;
}
