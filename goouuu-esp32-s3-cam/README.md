# GOOUUU ESP32-S3-CAM

Firmware for the **GOOUUU ESP32-S3-CAM** board (ESP32-S3 + OV2640 camera on module).

## Projects

| Project | Description | Docs |
|---------|-------------|------|
| [**mini-games/**](mini-games/) | 12 native arcade games — 128×64 OLED + 5 buttons (no camera, SD, Wi-Fi, or audio) | [README](mini-games/README.md) |
| [**rc-car/**](rc-car/) | 4WD seguidor IA — camera + LM Studio (Ministral 3) | [README](rc-car/README.md) |

## Requirements

- [PlatformIO](https://platformio.org/)
- [Yarn](https://yarnpkg.com/) 1.x
- SSD1306 I2C OLED (3.3 V) + 5 tactile buttons

## Quick start

From the monorepo root:

```bash
yarn mini-games:setup    # Linux, once — udev
yarn mini-games:build
yarn mini-games:flash
yarn mini-games:monitor
```

Or inside the project:

```bash
cd mini-games
yarn fw:setup
yarn fw:flash
yarn fw:monitor
```

Native USB CDC serial (GPIO 19/20). See [mini-games/README.md](mini-games/README.md) for wiring and power.
