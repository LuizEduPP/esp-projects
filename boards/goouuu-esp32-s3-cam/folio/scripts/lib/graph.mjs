import { randomUUID } from "node:crypto";
import { insertGraphEdge, insertGraphNode } from "./db.mjs";

function nodeId(day, kind, label) {
  const slug = label
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "-")
    .slice(0, 40);
  return `gn-${day}-${kind}-${slug}-${randomUUID().slice(0, 6)}`;
}

export function buildGraphFromEpisodes(db, day, episodes) {
  const nodes = new Map();

  const ensureNode = (kind, label, payload = {}) => {
    const key = `${kind}:${label}`;
    if (nodes.has(key)) {
      return nodes.get(key);
    }
    const id = nodeId(day, kind, label);
    const row = {
      id,
      day,
      kind,
      label,
      payload_json: JSON.stringify(payload),
    };
    insertGraphNode(db, row);
    nodes.set(key, id);
    return id;
  };

  for (const ep of episodes) {
    const summary = ep.summary ?? JSON.parse(ep.summary_json || "{}");
    const epNode = ensureNode("episode", summary.label || ep.label || ep.id, {
      started_at: ep.started_at,
      ended_at: ep.ended_at,
    });

    for (const theme of summary.themes ?? []) {
      const themeNode = ensureNode("theme", theme);
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: themeNode,
        relation: "themed",
        evidence_json: JSON.stringify([`ep:${ep.id}`]),
        confidence: 0.8,
      });
    }

    for (const d of summary.decisions ?? []) {
      const decNode = ensureNode("decision", d.text, { confidence: d.confidence });
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: decNode,
        relation: "decided",
        evidence_json: JSON.stringify(d.evidence ?? []),
        confidence: d.confidence ?? 0.7,
      });
    }

    for (const q of summary.open_questions ?? []) {
      const qNode = ensureNode("question", q);
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: qNode,
        relation: "left_open",
        evidence_json: JSON.stringify([`ep:${ep.id}`]),
        confidence: 0.75,
      });
    }

    for (const r of summary.rejected ?? []) {
      const rNode = ensureNode("rejected", r);
      insertGraphEdge(db, {
        day,
        from_node: epNode,
        to_node: rNode,
        relation: "rejected",
        evidence_json: JSON.stringify([`ep:${ep.id}`]),
        confidence: 0.9,
      });
    }
  }

  return { nodeCount: nodes.size };
}
