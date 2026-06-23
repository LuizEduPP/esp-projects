# Folio Brain — architecture

`scripts/lib/` is organized by domain. Each folder exposes a public API via `index.mjs`.

## Layout rule

```
lib/<domain>/index.mjs     ← public API
lib/<domain>/*.mjs         ← implementation
lib/*.mjs                  ← small shared modules (util, speaker, stt)
```

Prefer folders over loose files at `lib/` root (exception: consolidated flat modules).

## Tree (current)

```
lib/
├── util.mjs, speaker.mjs, stt.mjs
├── config/index.mjs
├── locale/index.mjs
├── models/index.mjs
├── present/index.mjs
├── db/                      # schema + repos (14 files — candidate for merge)
├── http/index.mjs, server.mjs
├── llm/                     # client, openai, scene, catalog
├── memory/                  # indexing, embeddings, retrieval, lexical
├── perception/              # audio, frame, image, sound, yamnet, scene
└── services/
    ├── ingest/index.mjs
    ├── pipeline/index.mjs
    └── insights/index.mjs
```

## Entry-point imports

```javascript
import { CFG } from "./lib/config/index.mjs";
import { openDb } from "./lib/db/index.mjs";
import { startProcessingLoop, startInsightsLoop } from "./lib/services/index.mjs";
import { createFolioServer } from "./lib/http/index.mjs";
import { retrieveMemories } from "./lib/memory/index.mjs";
import { timelineWithGroups } from "./lib/present/index.mjs";
```

## Data flow

```
ESP32 → services/ingest → db/
              ↓
       services/pipeline → perception/ + stt/ + llm/
              ↓
       services/insights → memory/ + llm/
              ↓
       present/ + http/ → UI
```

## Config

Single source: `~/.folio/config.json` + `FOLIO_*` env overrides. ESP pulls runtime fields via `GET /api/node/config` (frames, audio energy gates, `perception.motionMin`, node timing).

Sound classification: `perception.soundEngine` = `heuristic` | `yamnet`. Thresholds and labels live in config, not code.
