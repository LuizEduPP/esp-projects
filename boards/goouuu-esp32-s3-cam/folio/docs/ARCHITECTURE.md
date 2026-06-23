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
├── config.mjs, locale.mjs, models.mjs
├── db.mjs, llm.mjs, memory.mjs, http.mjs, services.mjs
├── util.mjs, speaker.mjs, stt.mjs, present.mjs
└── perception/          # frame, audio, sound, yamnet
```

## Entry-point imports

```javascript
import { CFG } from "./lib/config.mjs";
import { openDb } from "./lib/db.mjs";
import { startProcessingLoop, startInsightsLoop } from "./lib/services.mjs";
import { createFolioServer } from "./lib/http.mjs";
import { retrieveMemories } from "./lib/memory.mjs";
import { timelineWithGroups } from "./lib/present.mjs";
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
