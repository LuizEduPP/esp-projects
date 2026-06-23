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

## Offline spool (SD card)

The ESP **always** writes captures to the microSD (`/folio/audio`, `/folio/frames`). If WiFi or the brain is down, files stay on the card. When the network returns, the node drains the oldest pending files first (with backoff on HTTP errors — e.g. brain hot reload).

SD_MMC 1-bit pins (GOOUUU v1.3): CLK **39**, CMD **38**, D0 **40**. Insert a FAT32 card before power-on.

## Brain environment variables

| Var | Default | Description |
|-----|---------|-------------|
| `FOLIO_PORT` | `8770` | Brain HTTP port |
| `FOLIO_DATA_DIR` | `~/.folio` | SQLite + audio + frames |
| `LM_URL` | `http://127.0.0.1:1234/v1/chat/completions` | LM Studio |
| `FOLIO_MODEL_FAST` | `mistralai/ministral-3-3b` | Ingest + passes A/C |
| `FOLIO_MODEL_DEEP` | same | Passes B/D (use a larger model if available) |
| `FOLIO_WHISPER_BIN` | `whisper` | OpenAI Whisper CLI |
| `FOLIO_WHISPER_MODEL` | `base` | Whisper model |
| `FOLIO_EPISODE_GAP_MIN` | `12` | Minutes of silence → new episode |
| `FOLIO_DIGEST_INTERVAL_MS` | `1800000` | How often brain checks whether to refresh digest |
| `FOLIO_DIGEST_AUTO` | `1` | Set `0` to disable automatic digest |
| `FOLIO_LOCALE` | `pt-BR` | **Prompt + digest language** (BCP-47: `en-US`, `es-ES`, `fr-FR`, …) |
| `FOLIO_WHISPER_LANGUAGE` | *(from locale)* | Whisper `--language` override (e.g. `Portuguese`, `English`) |

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
- Raw audio in `~/.folio/audio/` — configure retention manually

## Speaker enrollment (optional)

```bash
yarn folio:enroll luiz "Luiz Eduardo"
```

Metadata only; voice embeddings are a future extension.

## Memory

All witness data and digests live in `~/.folio/` (`folio.db`, audio, frames, digests). Day continuity uses `day_rollups` and `profile_facts` inside that store.
