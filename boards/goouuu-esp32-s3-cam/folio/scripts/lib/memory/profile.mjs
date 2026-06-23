import { upsertProfileFact } from "../db.mjs";
import { iterDigestProfileFacts, profileSlug } from "./digest-facts.mjs";

export function syncProfileFromDigest(db, day, passB, passC) {
  for (const fact of iterDigestProfileFacts(passB, passC)) {
    upsertProfileFact(db, profileSlug(fact.category, fact.text), fact.text, day, fact.confidence);
  }
}
