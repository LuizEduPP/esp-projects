# Folio — schema & processing contract

Data directory: `~/.folio/` (override with `FOLIO_DATA_DIR`).

## Files on disk

```
~/.folio/
├── config.json
├── folio.db
├── audio/YYYY-MM-DD/*.pcm
├── frames/YYYY-MM-DD/*.jpg
├── models/              # YAMNet ONNX (when soundEngine=yamnet)
└── speakers/            # enrollment embeddings
```

## SQLite tables

### Ingest

| Table | Purpose |
|-------|---------|
| `devices` | ESP nodes (`folio-s3-01`, …) + last config sync |
| `audio_chunks` | Raw PCM paths, seq, energy, sound classification, `processed` |
| `utterances` | STT output linked to chunk |
| `frames` | JPEG paths + vision caption + `scene_json` |
| `events` | boot, `audio`, `frame`, `presence`, sound events |

### Structure

| Table | Purpose |
|-------|---------|
| `speakers` | Enrollment metadata + embedding path |
| `entities` | People, pets, places — from insights or manual enroll |
| `day_insights` | One row per day: stats + LM insights JSON |
| `memory_chunks` | RAG index — frame/utterance/sound snippets + optional embedding |

### Removed (legacy)

These tables are dropped on migrate: `episodes`, `episode_*`, `graph_*`, `digests`, `day_rollups`, `profile_facts`.

## Evidence ID convention

| Prefix | Refers to |
|--------|-----------|
| `utt:{id}` | `utterances.id` |
| `frm:{id}` | `frames.id` |
| `evt:{id}` | `events.id` |

## Processing loops

Two paths — **ingest is fast**, **Whisper/LM drain a pending queue**.

### Ingest (no LM)

1. ESP pushes PCM → file + `audio_chunks` (`processed=0`) + `events` kind `audio`
2. Speech/sound energy gate on ESP; brain re-classifies on pipeline
3. ESP pushes JPEG → file + `frames` (`processed=0`) + `events` kind `frame`

### Pipeline worker

Runs every `pipeline.intervalMs` (default 30s). Key config keys:

| Config key | Env override | Meaning |
|------------|--------------|---------|
| `pipeline.intervalMs` | `FOLIO_PIPELINE_INTERVAL_MS` | Worker wake interval |
| `audio.pipelineBatch` | `FOLIO_PIPELINE_AUDIO_BATCH` | Whisper chunks per tick |
| `frames.pipelineBatch` | `FOLIO_PIPELINE_FRAME_BATCH` | LM frames per tick |
| `frames.captionIntervalMs` | `FOLIO_FRAME_CAPTION_MS` | Min gap between LM vision calls |
| `frames.backlogGapMs.*` | — | Adaptive caption gap when queue is long |
| `pipeline.enabled` | `FOLIO_PIPELINE` | Set `false` for ingest-only brain |

- **Audio:** energy gate → sound classify (heuristic or YAMNet) → Whisper if speech → `utterances` → mark `processed`
- **Frames:** motion/quality gate → LM vision caption → mark `processed`
- LM failures leave rows in the queue for retry

Manual drain: `yarn folio:process`

### Insights worker

Runs every `insights.intervalMs` (default 5 min) when `insights.auto` is true:

1. `indexDayMemories` — build `memory_chunks` for RAG
2. `runDayInsights` — deep LM pass → `day_insights` + update `entities`

Entity links from sounds use `entities.soundKindEntity` in config (empty = no auto-link).

### Media API

| Route | Returns |
|-------|---------|
| `GET /api/frame/:id` | JPEG from `frames` row |
| `GET /api/audio/:id` | 16 kHz mono WAV from `audio_chunks` PCM |

## Continuity across days

1. **RAG (`memory_chunks`)** — retrieve top similar chunks from past N days (`memory.lookbackDays`)
2. **`day_insights`** — compact stats + narrative for the UI
3. **`entities`** — patterns updated from speaker IDs and LM output

Backfill: `yarn folio:memory:reindex`
