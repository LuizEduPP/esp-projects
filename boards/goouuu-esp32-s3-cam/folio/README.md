# Folio — passive day witness

The ESP32 **does not respond**. It listens and sees; the PC processes, stores, and writes an **interpreted chronicle** of your day.

## Stack

| Layer | Role |
|-------|------|
| **folio-node** (ESP32-S3-CAM + INMP441) | Push 1 s audio + JPEG every 60 s (+ motion) |
| **folio-brain** (PC, Node 22+) | Ingest, Whisper STT, LM Studio vision, `node:sqlite` |
| **Digest A→D** | Facts → interpretation → critique → prose |

## Hardware

- GOOUUU ESP32-S3-CAM (onboard camera)
- INMP441 I2S — GPIO 1 (WS), 2 (SCK), 42 (SD)
- Motion sensor (optional) — GPIO 3

No speaker, no OLED.

## Firmware configuration

Edit `firmware/platformio.ini`:

- `WIFI_SSID` / `WIFI_PASS`
- `FOLIO_BRAIN_URL` — PC IP, e.g. `http://192.168.1.100:8770`
- `FOLIO_DEVICE_ID` — stable node id

## Quick start

```bash
# From monorepo root
yarn folio:brain          # terminal 1 — ingest + UI http://localhost:8770

yarn folio:flash          # terminal 2 — flash ESP
yarn folio:monitor        # node logs

# After collecting data for the day
yarn folio:digest         # A→D pipeline + saves ~/.folio/digests/YYYY-MM-DD.md
```

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
| `FOLIO_LOCALE` | `pt-BR` | Digest output locale |

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

## Memory store

Folio uses its own local store (`~/.folio/`, `node:sqlite`). It does **not** integrate with [rememb](https://github.com/LuizEduPP/Rememb); day continuity is handled via `day_rollups` and `profile_facts` inside folio.db.
