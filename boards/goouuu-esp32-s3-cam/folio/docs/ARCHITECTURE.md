# Folio Brain — architecture

`scripts/lib/` is flat by domain, with `perception/` as the only subfolder.

## Layout

```
lib/
├── config.mjs, locale.mjs, models.mjs, present.mjs
├── db.mjs, llm.mjs, memory.mjs, http.mjs, services.mjs
├── util.mjs, speaker.mjs, stt.mjs
└── perception/          # audio, frame, sound, yamnet (+ index.mjs barrel)
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
ESP32 → services.mjs (ingest) → db.mjs
              ↓
       services.mjs (pipeline) → perception/ + stt.mjs + llm.mjs
              ↓
       services.mjs (insights) → memory.mjs + llm.mjs
              ↓
       present.mjs + http.mjs → UI
```

## Config

Single source: `~/.folio/config.json` + `FOLIO_*` env overrides. ESP pulls runtime fields via `GET /api/node/config` (frames, audio energy gates, `perception.motionMin`, node timing).

Sound classification: `perception.soundEngine` = `heuristic` | `yamnet`. Thresholds and labels live in config, not code.
