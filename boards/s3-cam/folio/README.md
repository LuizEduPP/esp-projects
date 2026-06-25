# Folio

> Mic + camera → HTTP push. Firmware only — no brain in this repo.

## Commands

```bash
yarn folio:flash
yarn folio:monitor
```

Config: copy `firmware/platformio.local.ini.example` → `platformio.local.ini` (WiFi + brain URL).

## Hardware

- GOOUUU ESP32-S3-CAM + INMP441 (GPIO 1 / 2 / 21)
- Pin map: [HARDWARE.md](HARDWARE.md)

## What it sends

| Data | When | Endpoint |
|------|------|----------|
| Audio PCM | Above energy threshold | `POST {brain}/ingest/audio` |
| JPEG | Every 60 s (default) | `POST {brain}/ingest/frame` |

Local: `GET /health` · `GET /capture`

## Files

```
folio/
├── README.md
├── HARDWARE.md
├── package.json
└── firmware/
    ├── platformio.ini
    └── src/main.cpp
```
