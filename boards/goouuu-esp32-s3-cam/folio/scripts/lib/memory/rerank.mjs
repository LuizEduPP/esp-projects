import { CFG } from "../config/index.mjs";
import { rerankDocuments } from "../llm/models-catalog.mjs";

export async function applyRerank(query, candidates) {
  if (!CFG.memoryRerankEnabled || !candidates.length) {
    return candidates;
  }
  if (!CFG.memoryRerankModel) {
    console.warn("[memory] rerank enabled but no model configured — skipping");
    return candidates;
  }

  const docs = candidates.map((c) => c.text);
  try {
    const ranked = await rerankDocuments(query, docs, {
      model: CFG.memoryRerankModel,
      topN: CFG.memoryRerankTopK,
    });
    if (!ranked.length) {
      return candidates.slice(0, CFG.memoryRerankTopK);
    }

    const out = ranked
      .map((r) => {
        const item = candidates[r.index];
        if (!item) {
          return null;
        }
        return {
          ...item,
          score: r.score,
          rerank_score: r.score,
        };
      })
      .filter(Boolean);

    console.log(`[memory] rerank ${out.length}/${candidates.length} via ${CFG.memoryRerankModel}`);
    return out;
  } catch (err) {
    console.warn(`[memory] rerank failed (${err.message}) — using retrieval order`);
    return candidates.slice(0, CFG.memoryRetrieveLimit);
  }
}
