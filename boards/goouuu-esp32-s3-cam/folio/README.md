# Folio — passive environment archive

The ESP32 **does not respond**. It listens and sees; the PC stores **everything** in one SQLite archive (`~/.folio/folio.db`): speech, ambient sounds, frames, who was present — multiple people, dogs, doors, the room.

## Stack

| Layer | Role |
|-------|------|
| **folio-node** (ESP32-S3-CAM + INMP441) | Push audio chunks + JPEG over WiFi to brain |
| **folio-brain** (PC, Node 22+) | Ingest, LM Studio STT, sound classify, speaker ID, vision captions, RAG memory, daily insights |
| **Archive UI** | Insights (dia) + witness timeline (frames, fala, sons) |

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
| SD (DOUT) | **GPIO 21** | Module I2S data |

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

Edit `firmware/platformio.ini` or copy `firmware/platformio.local.ini.example` → `platformio.local.ini` (gitignored):

- **WiFi** — ESP scans and connects to the **strongest open network** (no password). Optional `WIFI_SSID` / `WIFI_PASS` fallback if no open AP is found.
- `FOLIO_BRAIN_URL` — PC IP, e.g. `http://192.168.1.100:8770`
- `FOLIO_DEVICE_ID` — stable node id

## Quick start

```bash
# From monorepo root
yarn brain              # ingest + worker + insights auto (~5 min)
yarn process            # força fila Whisper/captions
yarn insights           # força insights do dia
yarn enroll luiz Luiz --pcm sample.pcm   # speaker ID
yarn memory:reindex     # rebuild RAG index

yarn folio:flash          # terminal 2 — flash ESP
yarn folio:monitor        # node logs

# After collecting data for the day
# Insights run automatically inside folio-brain (~5 min) → day_insights in SQLite
```

`yarn folio:brain` restarts automatically when you edit files under `scripts/`. Whisper, pipeline, insights, and memory indexing run in the background (`pipeline.enabled`, `insights.auto` in config).

Debug CLI:

```bash
yarn folio:insights --today          # or: yarn folio:process, folio:enroll, folio:memory:reindex
```

`yarn folio:brain` uses `--watch` (dev). Production: `yarn folio:brain:prod`.

### Scripts layout

```
scripts/
├── folio-brain.mjs      # HTTP server + background loops
├── folio.mjs            # CLI
├── lib/
│   ├── config.mjs       # ~/.folio/config.json
│   ├── db/              # SQLite (split modules)
│   ├── db.mjs           # re-export db/
│   ├── services/        # ingest, pipeline, insights
│   ├── services.mjs     # re-export services/
│   ├── network.mjs      # brainUrl por subnet do ESP
│   ├── perception/      # frame, audio, sound
│   └── …
└── ui/
```

## Capture pipeline (WiFi only)

Speech/sound chunks (RMS ≥ threshold) and JPEG frames are **pushed directly** to folio-brain when WiFi is up. Quiet audio is dropped on the ESP. If WiFi or the brain is offline, captures are skipped (no local buffer).

Requires `FOLIO_BRAIN_URL` reachable from the ESP’s network.

## Configuration

Sem arquivo de config obrigatório. Na subida o brain detecta LM Studio (localhost/LAN), **Whisper CLI no PC** (CUDA se tiver NVIDIA), modelo de embedding e idioma do sistema. Na UI (**Settings**) você só escolhe o **modelo** de visão/texto.

Opcional: `LM_URL`, `FOLIO_LM_MODEL`, `FOLIO_LOCALE`, `FOLIO_PORT`, `FOLIO_DATA_DIR`.

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/config` | Modelo salvo + runtime detectado |
| PUT | `/api/config` | `{"lm":{"model":"..."}}` |
| GET | `/api/openai/models` | Modelos no LM Studio |
| GET | `/api/node/config` | ESP pulls this (include `X-Folio-Device-Id`) |
| GET | `/api/devices` | Sync status per node |

ESP polls `/api/node/config` every 15s. Frame interval, JPEG quality, audio gates sync at runtime; buffer size / resolution need `platformio.ini` + reflash.

### ESP (`firmware/platformio.ini` build_flags)

| Flag | Default | Description |
|------|---------|-------------|
| `FOLIO_FRAME_INTERVAL_MS` | `60000` | JPEG every N ms |
| `FOLIO_JPEG_QUALITY` | `12` | Camera JPEG quality |
| `FOLIO_FRAME_SIZE_ID` | `6` | `6`=QVGA, `7`=VGA, `8`=SVGA |
| `FOLIO_CHUNK_MS` | `1000` | Audio chunk duration |
| `FOLIO_SAMPLE_RATE` | `16000` | I2S sample rate |
| `FOLIO_SPEECH_ENERGY` | `0.008` | Boot default — overridden at runtime from brain config |
| `FOLIO_SOUND_ENERGY` | `0.003` | Boot default — overridden at runtime from brain config |
| `FOLIO_MOTION_MIN` | `0.08` | Boot default — overridden at runtime from `perception.motionMin` |

After changing ESP flags: `yarn folio:flash`

## Local endpoints

### ESP (folio-node)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/health` | Diagnostics |
| GET | `/capture` | JPEG (manual pull) |

### Brain (folio-brain)

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/health` | Brain status + pending queue |
| GET | `/api/audio/:id` | WAV replay (410 after PCM retention) |
| GET | `/api/frame/:id` | JPEG |

Continuous push to the brain: `/ingest/audio`, `/ingest/frame`, `/ingest/event`. No pause — capture runs until the node or brain is stopped.

## Daily insights

Full contract: [docs/SCHEMA.md](docs/SCHEMA.md)

Every ~5 minutes (when `insights.auto` is true), the brain:

1. Indexes witness data into `memory_chunks` (RAG)
2. Runs a deep LM pass → `day_insights` (stats + narrative + entities)
3. Updates `entities` from speakers and LM output

Timeline grouping (speech/scene/sound gaps) is configured under `present.*`.

## Privacy

- **Continuous** capture while the ESP is powered and the brain is running — no pause in firmware
- To stop: power off the node or exit `yarn folio:brain`
- Only speech chunks (RMS ≥ threshold) are pushed, stored, and transcribed. Quiet audio is dropped on ESP and at ingest.
- PCM files are kept for `audio.retentionDays` (default 7) for replay; then deleted while **transcripts remain** in SQLite.

## Memory & RAG

Persistent memory lives in `~/.folio/folio.db`:

| Store | Role |
|-------|------|
| `memory_chunks` | Indexed frames, utterances, sounds — **retrieved by similarity** for insights context |
| `day_insights` | Daily stats + LM narrative |
| `entities` | People, pets, patterns |

After each insights run: index today's witness → `memory_chunks`. Set `memory.useEmbeddings: true` in config only if your server exposes `/v1/embeddings`.

```bash
yarn folio:memory:reindex

# Search memory
curl 'http://localhost:8770/api/memory?q=esp32+lm+studio'
```

Optional semantic embeddings: `"memory.useEmbeddings": true` (requires `/v1/embeddings` on the same base URL). Default is lexical cosine (no extra model).

## Speaker enrollment (optional)

```bash
node scripts/folio.mjs enroll luiz "Luiz Eduardo"
```

Metadata only; voice embeddings are a future extension.

## Data layout

All witness data and insights live in `~/.folio/`. See [Memory & RAG](#memory--rag) above.
