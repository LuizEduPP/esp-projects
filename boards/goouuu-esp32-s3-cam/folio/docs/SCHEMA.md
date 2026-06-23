# Folio — schema & digest contract

Data directory: `~/.folio/` (override with `FOLIO_DATA_DIR`).

## Files on disk

```
~/.folio/
├── folio.db
├── audio/YYYY-MM-DD/*.pcm
├── frames/YYYY-MM-DD/*.jpg
├── digests/YYYY-MM-DD.md
└── speakers/          # future embeddings
```

## SQLite tables

### Ingest

| Table | Purpose |
|-------|---------|
| `devices` | ESP nodes (`folio-s3-01`, …) |
| `audio_chunks` | Raw PCM paths, seq, energy, `processed` |
| `utterances` | STT output linked to chunk |
| `frames` | JPEG paths + vision caption + `scene_json` |
| `events` | boot, motion, pause, resume |

### Structure

| Table | Purpose |
|-------|---------|
| `episodes` | Time-bounded slices of the day + `summary_json` |
| `episode_utterances` | M:N utterance ↔ episode |
| `episode_frames` | M:N frame ↔ episode (±60 s alignment) |
| `graph_nodes` | themes, decisions, questions, rejected ideas |
| `graph_edges` | relations with `evidence_json` |

### Output

| Table | Purpose |
|-------|---------|
| `digests` | One row per day: `pass_a_json` … `pass_d` as `prose` |
| `day_rollups` | Compact JSON for next-day continuity |
| `profile_facts` | Learned patterns (`pattern:*` keys) |
| `speakers` | Enrollment metadata |

## Evidence ID convention

| Prefix | Refers to |
|--------|-----------|
| `utt:{id}` | `utterances.id` |
| `frm:{id}` | `frames.id` |
| `ep:{id}` | `episodes.id` |
| `evt:{id}` | `events.id` |

Passes must cite these IDs. Pass C drops claims without backing.

## Pass contracts (JSON shape)

### Pass A — `pass_a_json`

```json
{
  "timeline": [{ "at": "ISO8601", "fact": "string", "evidence": ["utt:1"] }],
  "episode_facts": [{ "episode_id": "ep-…", "facts": ["string"] }],
  "events_notable": ["string"]
}
```

### Pass B — `pass_b_json`

```json
{
  "narrative_arc": "string",
  "shifts": [{ "at": "ISO8601", "description": "string", "evidence": [] }],
  "decisions_real": [{ "text": "string", "evidence": [] }],
  "open_loops": ["string"],
  "patterns": ["string"],
  "tomorrow_pull": ["string"]
}
```

### Pass C — `pass_c_json`

```json
{
  "approved_claims": [{ "text": "string", "evidence": [], "confidence": 0.9 }],
  "rejected_claims": [{ "text": "string", "reason": "string" }],
  "evidence_gaps": ["string"]
}
```

### Pass D — `prose`

Plain text (pt-BR). No `##` headings. Final line:

`*Evidência: utt:12, frm:3, ep:…*`

## Episode `summary_json` (per episode, pre-digest)

```json
{
  "label": "short title",
  "decisions": [{ "text": "", "confidence": 0.9, "evidence": [] }],
  "rejected": ["ideas abandoned"],
  "open_questions": [],
  "themes": [],
  "energy": "focused",
  "visual_context": "one line",
  "implicit_shifts": [],
  "notable_quotes": [{ "text": "", "evidence": [] }]
}
```

## Processing loop

1. ESP pushes PCM → `audio_chunks`
2. Brain: energy gate → Whisper → `utterances`
3. ESP pushes JPEG → `frames` → LM vision → caption
4. On digest: cluster utterances → episodes → graph → passes A–D

## Continuity across days

Before Pass B/D, brain loads `day_rollups` for **previous calendar day** (`compact_json`: arc, decisions, open_loops, patterns).
