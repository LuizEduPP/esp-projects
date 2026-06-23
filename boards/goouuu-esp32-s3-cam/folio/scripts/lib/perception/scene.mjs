import { MotionLevel } from "./types.mjs";

export function enrichScene(scene, { motion, quality } = {}) {
  const out = { ...scene };
  if (motion) {
    out.motion_score = motion.score;
    out.motion_level = motion.level ?? MotionLevel.NONE;
  }
  if (quality) {
    out.brightness = quality.brightness;
    out.lighting = quality.dark ? "dark" : quality.bright ? "bright" : "normal";
    if (quality.dark) {
      out.enhanced = true;
    }
  }
  if (Array.isArray(out.objects)) {
    out.object_tags = out.objects
      .map((o) => (typeof o === "string" ? o : o?.name))
      .filter(Boolean)
      .slice(0, 6);
  }
  return out;
}

export function objectsSummary(scene) {
  const tags = scene?.object_tags ?? scene?.objects ?? [];
  if (!tags.length) {
    return null;
  }
  return tags
    .map((o) => (typeof o === "string" ? o : o?.name))
    .filter(Boolean)
    .slice(0, 4)
    .join(", ");
}
