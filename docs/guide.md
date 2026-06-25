# Guide

Everything you need to build and flash firmware in this repo.

## Prerequisites

- [PlatformIO](https://platformio.org/) (IDE extension or `pio` on PATH)
- [Yarn](https://yarnpkg.com/) 1.x
- USB cable with data lines

## First time (Linux)

```bash
yarn install
yarn setup          # udev + dialout — log out/in after
```

Check port: `ls /dev/ttyACM* /dev/ttyUSB*`

| Board | Typical port |
|-------|--------------|
| S3-CAM (USB-C) | `/dev/ttyACM0` |
| S3-CAM (USB-TTL) / CYD | `/dev/ttyUSB0` |

Override in `firmware/platformio.local.ini` (gitignored).

## How the repo is organized

**Two boards, five apps:**

```
boards/s3-cam/          GOOUUU ESP32-S3-CAM + camera
  mini-games/           128×64 OLED + 5 buttons
  rc-car/               4WD + camera + PC (LM Studio)
  folio/                mic + camera → HTTP push

boards/cyd/             Cheap Yellow Display (2.8″ touch)
  gb/                   Game Boy / GBC emulator (needs SD)
  arcade/               12 embedded games (no SD)
```

Every app has the same skeleton:

```
<app>/
├── README.md       what it does + wiring
├── package.json    fw:build, fw:flash, fw:monitor
├── firmware/       PlatformIO (C++)
└── scripts/        optional — host scripts (Node/Python)
```

Shared build tools live in repo root `scripts/`.

## Flash an app

From repo root (shortcuts):

```bash
yarn mini-games:flash && yarn mini-games:monitor
yarn gb:flash && yarn gb:monitor
```

Or from inside the app:

```bash
cd boards/s3-cam/mini-games
yarn fw:flash
yarn fw:monitor
```

## Adding a new app

1. Copy an existing sibling folder under the right board.
2. Edit `package.json` → new workspace `name`.
3. Add a row to root [README.md](../README.md).
4. Add root shortcut in root `package.json` (optional).

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `pio: not found` | Install PlatformIO extension or `pip install platformio` |
| Permission denied on serial | `yarn setup`, re-login, check `groups` has `dialout` |
| Upload fails | Check `upload_port` in `platformio.local.ini` |
| CYD SPIFFS errors | `yarn gb:install` or `yarn arcade:install` (erase + flash) |

## Hardware list

See [hardware-inventory.md](hardware-inventory.md).
