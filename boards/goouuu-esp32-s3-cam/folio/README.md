# Folio — passive day witness

The ESP32 **does not respond**. It listens and sees; the PC processes, stores, and writes an **interpreted chronicle** of your day.

## Stack

| Layer | Role |
|-------|------|
| **folio-node** (ESP32-S3-CAM + INMP441) | Push 1 s audio + JPEG every 60 s (USB power) |
| **folio-brain** (PC, Node 22+) | Ingest, presence from camera/audio, Whisper, LM Studio, `node:sqlite` |
| **Digest A→D** | Facts → interpretation → critique → prose |

## Hardware

- GOOUUU ESP32-S3-CAM (onboard camera)
- INMP441 I2S — GPIO 1 (WS), 2 (SCK), 42 (SD)
- USB power (no external PIR / presence sensor)

No speaker, no OLED.

## Wiring — INMP441 (I2S)

Source of truth: `firmware/include/pins.h`

| INMP441 pin | ESP32-S3-CAM | Notes |
|-------------|--------------|-------|
| VDD | **3.3 V** | Not 5 V |
| GND | **GND** | |
| SCK (BCLK) | **GPIO 2** | I2S bit clock |
| WS (LRCLK) | **GPIO 1** | I2S word select |
| SD (DOUT) | **GPIO 42** | Mic → ESP data in |
| L/R | **GND** | Left channel (mono) |

I2S: 16 kHz, 16-bit, mono left. On boot, serial should show `[audio] INMP441 OK 16kHz mono`.

**Power:** USB-C only (for now).

### Camera

Uses onboard OV2640 — no extra wiring. GPIO map in `pins.h` (`CAM_PIN_*`).

## Presence (brain only)

No PIR on the device. **folio-brain** infers activity from:

| Source | Signal |
|--------|--------|
| **Camera** | Vision caption → `person_present` / `people` → `presence` event |
| **Audio** | Non-silent chunk (energy gate) → `presence` event + Whisper STT |

The ESP always streams; presence logic runs on the PC.

**ESP only pushes.** The brain stores everything, drains the pending queue (Whisper + LM), and serves the UI. The node does not wait for captions or transcripts.

## Witness UI

Open `http://localhost:8770` (or your PC LAN IP). The log shows:

| Capture | What you see |
|---------|----------------|
| **Frame** | JPEG thumbnail (click to open full size) + LM description when processed |
| **Audio** | 1 s WAV player + Whisper transcript when processed |

Media API (same host): `GET /api/frame/:id`, `GET /api/audio/:id`.

## Firmware configuration

Edit `firmware/platformio.ini`:

- `WIFI_SSID` / `WIFI_PASS`
- `FOLIO_BRAIN_URL` — PC IP, e.g. `http://192.168.1.100:8770`
- `FOLIO_DEVICE_ID` — stable node id

## Quick start

```bash
# From monorepo root
yarn folio:brain          # terminal 1 — ingest + UI (hot reload on script changes)

yarn folio:flash          # terminal 2 — flash ESP (SD card required for offline spool)
yarn folio:monitor        # node logs

# After collecting data for the day
# Digest runs automatically inside folio-brain (A→D, saves ~/.folio/digests/YYYY-MM-DD.md)
```

`yarn folio:brain` restarts automatically when you edit files under `scripts/`. Use `yarn workspace goouuu-s3-cam-folio brain:once` for a single run without watch.

Manual overrides: `yarn folio:digest`, `yarn workspace goouuu-s3-cam-folio brain:process`, `yarn folio:enroll`.

### Scripts layout

```
scripts/
├── folio-brain.mjs      # HTTP server + background loops
├── folio.mjs            # CLI: digest | process | enroll
├── ui/index.html
└── lib/
    ├── config.mjs       # ~/.folio/config.json + env
    ├── db.mjs           # SQLite schema + queries
    ├── http.mjs         # routes (/ingest, /api/*)
    ├── ingest.mjs       # fast path: save PCM/JPEG
    ├── worker.mjs       # Whisper + LM queue drain
    ├── retention.mjs    # audio file cleanup
    ├── digest/
    │   ├── passes.mjs   # A→D pipeline
    │   ├── compact.mjs
    │   └── scheduler.mjs
    ├── memory/          # RAG index + retrieve
    ├── episodes.mjs
    ├── graph.mjs
    ├── lm.mjs
    ├── whisper.mjs
    ├── locale.mjs
    └── serve.mjs
```

## Offline spool (SD card)

The ESP **always** writes captures to the microSD (`/folio/audio`, `/folio/frames`). If WiFi or the brain is down, files stay on the card. When the network returns, the node drains the oldest pending files first (with backoff on HTTP errors — e.g. brain hot reload).

SD_MMC 1-bit pins (GOOUUU v1.3): CLK **39**, CMD **38**, D0 **40**. Insert a FAT32 card before power-on.

## Configuration

Copy the template and edit:

```bash
mkdir -p ~/.folio
cp boards/goouuu-esp32-s3-cam/folio/folio.config.example.json ~/.folio/config.json
```

**Env vars override** `config.json`. Edit in the UI (**Settings**) or via API:

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/config` | Full config + version |
| PUT | `/api/config` | Save (partial JSON merge) → reloads brain hot settings |
| GET | `/api/node/config` | ESP pulls this (include `X-Folio-Device-Id`) |
| GET | `/api/devices` | Sync status per node |

ESP polls `/api/node/config` every `node.statusIntervalMs` and sends `X-Folio-Config-Version` on ingest. Frame interval + JPEG quality apply at runtime; audio buffer size / frame resolution need matching `platformio.ini` + reflash.

### Brain (`~/.folio/config.json`)

| Key | Env override | Default | Description |
|-----|----------------|---------|-------------|
| `locale` | `FOLIO_LOCALE` | `pt-BR` | LM prompts + digest language |
| `frames.captureIntervalMs` | `FOLIO_FRAME_INTERVAL_MS` | `60000` | Doc only — ESP capture rate |
| `frames.captionIntervalMs` | `FOLIO_FRAME_CAPTION_MS` | `60000` | Min gap between LM vision calls |
| `frames.captionMaxTokens` | `FOLIO_FRAME_CAPTION_MAX_TOKENS` | `220` | Tokens per frame caption |
| `frames.pipelineBatch` | `FOLIO_PIPELINE_FRAME_BATCH` | `1` | Frames per pipeline tick |
| `frames.jpegQuality` | `FOLIO_JPEG_QUALITY` | `12` | Doc — ESP JPEG quality (lower=better) |
| `frames.size` | `FOLIO_FRAME_SIZE` | `QVGA` | Doc — `QVGA`, `VGA`, `SVGA` |
| `audio.chunkMs` | `FOLIO_AUDIO_CHUNK_MS` | `1000` | PCM chunk length |
| `audio.speechEnergyThreshold` | `FOLIO_SPEECH_ENERGY` | `0.008` | Gate for speech / presence |
| `audio.whisperModel` | `FOLIO_WHISPER_MODEL` | `base` | Whisper model size |
| `audio.pipelineBatch` | `FOLIO_PIPELINE_AUDIO_BATCH` | `4` | Chunks per worker tick |
| `audio.retentionDays` | `FOLIO_AUDIO_RETENTION_DAYS` | `7` | Keep PCM with transcript |
| `pipeline.intervalMs` | `FOLIO_PIPELINE_INTERVAL_MS` | `30000` | Queue worker interval |
| `digest.intervalMs` | `FOLIO_DIGEST_INTERVAL_MS` | `1800000` | Auto digest check interval |
| `episodes.gapMin` | `FOLIO_EPISODE_GAP_MIN` | `12` | Silence minutes → new episode |
| `lm.modelFast` / `modelDeep` | `FOLIO_MODEL_*` | ministral-3-3b | LM Studio models |
| `lm.url` | `LM_URL` | `127.0.0.1:1234` | LM Studio API |

### ESP (`firmware/platformio.ini` build_flags)

| Flag | Default | Description |
|------|---------|-------------|
| `FOLIO_FRAME_INTERVAL_MS` | `60000` | JPEG every N ms |
| `FOLIO_JPEG_QUALITY` | `12` | Camera JPEG quality |
| `FOLIO_FRAME_SIZE_ID` | `6` | `6`=QVGA, `7`=VGA, `8`=SVGA |
| `FOLIO_CHUNK_MS` | `1000` | Audio chunk duration |
| `FOLIO_SAMPLE_RATE` | `16000` | I2S sample rate |

After changing ESP flags: `yarn folio:flash`

## Local ESP endpoints

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/health` | Diagnostics |
| GET | `/capture` | JPEG (manual pull) |

Continuous push to the brain: `/ingest/audio`, `/ingest/frame`, `/ingest/event`. No pause — capture runs until the node or brain is stopped.

## Digest pipeline (A→D)

Full contract: [docs/SCHEMA.md](docs/SCHEMA.md)

1. **Pass A** — factual ledger with evidence IDs (`utt:`, `frm:`, `ep:`)
2. **Pass B** — arc, real decisions, open loops, patterns
3. **Pass C** — critique: drop claims without evidence
4. **Pass D** — prose letter (`FOLIO_LOCALE`), no markdown section templates

Episodes: speech clusters by configurable gap + multimodal semantic extraction.

## Privacy

- **Continuous** capture while the ESP is powered and the brain is running — no pause in firmware
- To stop: power off the node or exit `yarn folio:brain`
- Only speech PCM is stored (empty or below-threshold chunks are dropped at ingest). After Whisper, chunks with no transcript are deleted immediately. PCM with transcripts kept for `audio.retentionDays` (default 7).

## Memory & RAG

Persistent memory lives in `~/.folio/folio.db`:

| Store | Role |
|-------|------|
| `memory_chunks` | Indexed episodes, decisions, themes, claims, digest snippets — **retrieved by similarity** into Pass B/D |
| `graph_nodes` / `graph_edges` | Day graph — **matched by theme** across past days |
| `profile_facts` | Long-term patterns, decisions, open loops, approved claims |
| `day_rollups` | Yesterday's compact arc (fast continuity) |

After each digest: index today's witness → `memory_chunks`. Before Pass B/D: **RAG** pulls top matches from the last `memory.lookbackDays` (default 90).

```bash
# Backfill index from existing digests
yarn workspace goouuu-s3-cam-folio exec node scripts/folio.mjs memory reindex

# Search memory
curl 'http://localhost:8770/api/memory?q=esp32+lm+studio'
```

Optional semantic embeddings: `"memory.useEmbeddings": true` (LM Studio `/v1/embeddings`). Default is lexical cosine (no extra model).

## Speaker enrollment (optional)

```bash
yarn folio:enroll luiz "Luiz Eduardo"
```

Metadata only; voice embeddings are a future extension.

## Data layout

All witness data and digests live in `~/.folio/`. See [Memory & RAG](#memory--rag) above.
