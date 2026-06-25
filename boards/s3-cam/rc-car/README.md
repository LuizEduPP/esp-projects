# RC Car

> 4WD robot — ESP32 camera + LM Studio vision on PC.

## Commands

```bash
yarn rc-car:flash          # firmware
yarn rc-car:monitor
ESP_URL=http://<esp-ip> yarn rc-car:ai    # AI loop + panel :8765
yarn rc-car:diag           # motor test
```

Set WiFi in `firmware/platformio.ini` or `platformio.local.ini`.

## Hardware

| Part | Notes |
|------|-------|
| GOOUUU ESP32-S3-CAM | Camera + HTTP server |
| 2× L9110S | Left GPIO 1,2,14,47 · Right 21,48,19,20 |
| 4× DC motors | 4WD chassis |

Pins: [`firmware/include/pins.h`](firmware/include/pins.h)

## ESP endpoints

| Path | Purpose |
|------|---------|
| `GET /capture` | JPEG |
| `GET /diag` | GPIO levels |
| `POST /control` | Motors `{l, r}` −255..255 |

## Files

```
rc-car/
├── README.md
├── package.json
├── scripts/          ai-follow.mjs, motor-diag.mjs
└── firmware/
```
