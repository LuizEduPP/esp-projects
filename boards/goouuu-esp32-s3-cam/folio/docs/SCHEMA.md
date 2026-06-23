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
| `events` | boot, `audio`, `frame` (on ingest), `presence` (mic energy or LM camera) |

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
| `profile_facts` | Long-term patterns, decisions, open loops, claims |
| `memory_chunks` | RAG index — episode/decision/theme/digest snippets + embedding |
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

Plain text in the language set by `FOLIO_LOCALE` (default `pt-BR`). Applies to frame captions, episode labels, digest passes A–D, and Pass D prose. No `##` headings. Final line uses localized evidence label (`Evidência:` / `Evidence:` / …).

`*Evidence: utt:12, frm:3, ep:…*`

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

Two paths — **ingest is fast**, **LM/Whisper drain a pending queue**.

### Ingest (no LM)

1. ESP pushes PCM → file + `audio_chunks` (`processed=0`) + `events` kind `audio`
2. Speech energy → extra `presence` event (`source: audio`)
3. ESP pushes JPEG → file + `frames` (`processed=0`) + `events` kind `frame`

### Pending queue worker

Runs every `FOLIO_PIPELINE_INTERVAL_MS` (default 30s). Config:

| Env | Default | Meaning |
|-----|---------|---------|
| `FOLIO_PIPELINE_INTERVAL_MS` | 30000 | Worker wake interval |
| `FOLIO_PIPELINE_AUDIO_BATCH` | 2 | Whisper chunks per tick |
| `FOLIO_PIPELINE_FRAME_BATCH` | 1 | LM frames per tick |
| `FOLIO_FRAME_CAPTION_MS` | 60000 | Min gap between LM vision calls |
| `FOLIO_PIPELINE` | 1 | Set `0` for ingest-only brain |

- **Audio:** energy gate → Whisper → `utterances` → mark `processed`
- **Frames:** LM vision captions one pending frame per gap → mark `processed`
- LM failures leave the frame in the queue for retry

Manual drain: `yarn brain:process`

### Media API

| Route | Returns |
|-------|---------|
| `GET /api/frame/:id` | JPEG from `frames` row |
| `GET /api/audio/:id` | 16 kHz mono WAV from `audio_chunks` PCM |

### Digest (automatic)

The brain runs passes A→D **automatically** when new witness data arrives (checked every `FOLIO_DIGEST_INTERVAL_MS`, default 30 min). On day rollover it finalizes yesterday. Prose appears in the UI and `~/.folio/digests/YYYY-MM-DD.md`.

Witness payloads are **compacted** before each LM call (sampled moments, truncated text) to fit typical 16k context. If LM Studio still errors, reload the model with a larger context length.

Manual override: `yarn folio:digest` or `POST /api/digest/run?day=…`

## Continuity across days

1. **`day_rollups`** — previous calendar day compact JSON (arc, decisions, open loops)
2. **RAG (`memory_chunks`)** — before Pass B/D, retrieve top similar chunks from past 90 days (lexical or LM embeddings)
3. **`graph_nodes`** — theme/decision overlap from prior days
4. **`profile_facts`** — stable facts updated each digest

After Pass D, index the day into `memory_chunks` and sync `profile_facts`.

Backfill: `node scripts/folio.mjs memory reindex`
