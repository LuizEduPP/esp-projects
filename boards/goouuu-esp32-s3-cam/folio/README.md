# Folio — passive day witness

The ESP32 **does not respond**. It listens and sees; the PC processes, stores, and writes an **interpreted chronicle** of your day.

## Stack

| Layer | Role |
|-------|------|
| **folio-node** (ESP32-S3-CAM + INMP441) | Push 1 s audio + JPEG every 60 s (USB power) |
| **folio-brain** (PC, Node 22+) | Ingest, presence from camera/audio, Whisper, OpenAI-compatible API, `node:sqlite` |
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

Debug CLI:

```bash
yarn folio:digest --today          # or: yarn folio:process, folio:enroll, folio:memory:reindex
```

`yarn folio:brain` uses `--watch` (dev). Production: `yarn folio:brain:prod`.

### Scripts layout

```
scripts/
├── folio-brain.mjs      # HTTP server + background loops
├── folio.mjs            # CLI: digest | process | enroll | memory reindex
├── ui/index.html
└── lib/                     # só pastas — cada uma com index.mjs
    ├── config/
    ├── locale/
    ├── models/              # ModelSlot, modelId
    ├── db/
    ├── util/                # time, audio, response, json
    ├── stt/
    ├── llm/
    ├── memory/
    ├── services/            # ingest/, pipeline/, digest/
    └── http/
```

## Offline spool (microSD card — slot onboard)

**Always:** speech chunk (RMS ≥ threshold) → **microSD** → push to brain when WiFi is up.  
Quiet seconds are **not** spooled or pushed. On successful push, the file is removed from the card.

This is the **microSD slot** (CLK 39, CMD 38, D0 40) — not the INMP441 I2S data pin (DOUT).

Insert a **FAT32** card before power-on. Boot should show `[microsd] ok …MB /folio`.

## Configuration

Copy the template and edit:

```bash
mkdir -p ~/.folio
cp boards/goouuu-esp32-s3-cam/folio/folio.config.example.json ~/.folio/config.json
```

**Env vars override** `config.json` (`OPENAI_BASE_URL`, `OPENAI_API_KEY`, `OPENAI_MODEL`, `OPENAI_MODEL_DEEP`; legacy `LM_URL` / `FOLIO_LM_MODEL` still work). Edit in the UI (**Settings**) or via API:

| Method | Path | Purpose |
|--------|------|---------|
| GET | `/api/config` | Full config + version |
| PUT | `/api/config` | Save (partial merge) → hot reload OpenAI/Whisper/locale; **restart brain** only for `port` / `dataDir` |
| GET | `/api/openai/models` | List models from configured base URL (alias: `/api/lm/models`) |
| GET | `/api/node/config` | ESP pulls this (include `X-Folio-Device-Id`) |
| GET | `/api/devices` | Sync status per node |

ESP polls `/api/node/config` every `node.statusIntervalMs` and sends `X-Folio-Config-Version` on ingest. Frame interval + JPEG quality apply at runtime; audio buffer size / frame resolution need matching `platformio.ini` + reflash.

### Brain — UI settings (`Settings` panel)

| Key | Default | Description |
|-----|---------|-------------|
| `locale` | `pt-BR` | Language |
| `openai.baseUrl` / `openai.apiKey` / `openai.model` / `openai.modelDeep` | OpenAI API | LM Studio, Ollama, vLLM, OpenAI — fast = captions; deep = digest passes B/D |
| `audio.whisperModel` | `base` | Whisper model |
| `audio.whisperDevice` | `cuda` | `cpu`, `cuda`, `mps`, `auto` — see UI **runtime** line for effective device |
| `audio.speechEnergyThreshold` | `0.008` | RMS gate (ESP `FOLIO_SPEECH_ENERGY` must match) |
| `audio.retentionDays` | `7` | PCM replay window; transcripts kept longer |
| `pipeline.enabled` / `digest.auto` | `true` | Background worker + digest |
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
| `FOLIO_SPEECH_ENERGY` | `0.008` | RMS gate — must match `audio.speechEnergyThreshold` in brain config |

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
- Only speech chunks (RMS ≥ threshold) are spooled, stored, and transcribed. Quiet audio is dropped on ESP and at ingest.
- PCM files are kept for `audio.retentionDays` (default 7) for replay; then deleted while **transcripts remain** in SQLite.

## Memory & RAG

Persistent memory lives in `~/.folio/folio.db`:

| Store | Role |
|-------|------|
| `memory_chunks` | Indexed episodes, decisions, themes, claims, digest snippets — **retrieved by similarity** into Pass B/D |
| `graph_nodes` / `graph_edges` | Day graph — **matched by theme** across past days |
| `profile_facts` | Long-term patterns, decisions, open loops, approved claims |
| `day_rollups` | Yesterday's compact arc (fast continuity) |

After each digest: index today's witness → `memory_chunks`. Before Pass B/D: **RAG** pulls top lexical matches from the last 90 days + graph theme overlap + `profile_facts`. Set `memory.useEmbeddings: true` in config only if your server exposes `/v1/embeddings`.

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

All witness data and digests live in `~/.folio/`. See [Memory & RAG](#memory--rag) above.
