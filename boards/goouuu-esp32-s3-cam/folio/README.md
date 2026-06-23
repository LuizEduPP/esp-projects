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
- INMP441 I2S — **GPIO 1 (WS), 2 (SCK), 21 (DOUT)** — see [`firmware/PINOUT.md`](firmware/PINOUT.md) (map; Keyestudio kit uses GPIO42 = MTMS, wrong for Folio)

No speaker, no OLED.

## Wiring — INMP441 (Keyestudio KS5028 / kit S3-CAM)

GPIO **1**, **2**, and **21** are safe general-purpose pins on the ESP32-S3 — that is why Folio uses them for INMP441 I2S. **GPIO42** is MTMS/JTAG (caution); the Keyestudio kit diagram is wrong for this board. Full tables: [`firmware/PINOUT.md`](firmware/PINOUT.md).

| INMP441 | GOOUUU GPIO | note |
|---------|-------------|------------|
| VDD | **3.3 V** | |
| GND | **GND** | |
| **L/R** | **GND** | Tie L/R to the same GND rail |
| WS | **GPIO 1** | ADC1_0 |
| SCK | **GPIO 2** | ADC1_1 |
| SD (DOUT) | **GPIO 21** | Module I2S data — **not** the microSD slot (38/39/40) |

**L/R “junto”** = L/R tied to **GND**, not floating.

Boot: `[audio] OK DOUT GPIO21 ... pcmPeak=...` — when speaking, `pcmPeak` ~500–15000 (not 0, not 32767).

**Power:** USB-C only (for now).

### Camera

Uses onboard OV2640 — no extra wiring. GPIO map in `pins.h` (`CAM_PIN_*`) and full table in [`firmware/PINOUT.md`](firmware/PINOUT.md).

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
yarn folio:brain          # terminal 1 — ingest, Whisper, digest, UI (all automatic)

yarn folio:flash          # terminal 2 — flash ESP (SD card required for offline spool)
yarn folio:monitor        # node logs

# After collecting data for the day
# Digest runs automatically inside folio-brain (A→D, saves ~/.folio/digests/YYYY-MM-DD.md)
```

`yarn folio:brain` restarts automatically when you edit files under `scripts/`. Whisper, digest A→D, and memory indexing run in the background (`pipeline.enabled`, `digest.auto` in config).

Debug CLI (no yarn script): `node scripts/folio.mjs digest --today`, `process`, `enroll`, `memory reindex`.

### Scripts layout

```
scripts/
├── folio-brain.mjs      # HTTP server + background loops
├── folio.mjs            # CLI: digest | process | enroll | memory reindex
├── ui/index.html
└── lib/
    ├── config.mjs       # ~/.folio/config.json + CFG
    ├── db.mjs           # SQLite schema + queries + openDb
    ├── digest.mjs       # episodes, graph, passes A→D, scheduler
    ├── memory.mjs       # RAG embed/retrieve + index
    ├── http.mjs         # routes (/ingest, /api/*)
    ├── ingest.mjs       # fast path: save PCM/JPEG
    ├── worker.mjs       # Whisper + LM queue + retention
    ├── lm.mjs
    ├── whisper.mjs
    ├── locale.mjs
    └── util.mjs
```

## Offline spool (microSD card — slot onboard)

**Always:** capture → **microSD** (`/folio/audio`, `/folio/frames`) → push to brain when WiFi is up.  
On successful push, the file is removed from the card. Pending files drain oldest-first.

This is the **microSD slot** (CLK 39, CMD 38, D0 40) — not the INMP441 I2S data pin (DOUT).

Insert a **FAT32** card before power-on. Boot should show `[microsd] ok …MB /folio`.

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

### Brain — UI settings (`Settings` panel)

| Key | Default | Description |
|-----|---------|-------------|
| `locale` | `pt-BR` | Language |
| `lm.url` / `lm.model` | LM Studio | Vision + digest |
| `audio.whisperModel` | `base` | Whisper |
| `audio.speechEnergyThreshold` | `0.008` | Speech gate |
| `episodes.gapMin` | `12` | Episode split (min silence) |
| `frames.*` / `node.*` | — | ESP sync |

Everything else (`pipeline`, `digest`, `memory`, `worker`) runs automatically — tune in `~/.folio/config.json` only if needed.

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

After each digest: index today's witness → `memory_chunks`. Before Pass B/D: **RAG** pulls top lexical matches from the last 90 days + graph theme overlap + `profile_facts`. Set `memory.useEmbeddings: true` in config only if LM Studio exposes `/v1/embeddings`.

```bash
# Backfill index from existing digests
yarn workspace goouuu-s3-cam-folio exec node scripts/folio.mjs memory reindex

# Search memory
curl 'http://localhost:8770/api/memory?q=esp32+lm+studio'
```

Optional semantic embeddings: `"memory.useEmbeddings": true` (LM Studio `/v1/embeddings`). Default is lexical cosine (no extra model).

## Speaker enrollment (optional)

```bash
node scripts/folio.mjs enroll luiz "Luiz Eduardo"
```

Metadata only; voice embeddings are a future extension.

## Data layout

All witness data and digests live in `~/.folio/`. See [Memory & RAG](#memory--rag) above.
