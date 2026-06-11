# CYD-GB

**Game Boy / Game Boy Color** emulator for the [**ESP32-2432S028R**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) (Cheap Yellow Display). Fully touchscreen controlled — no extra physical buttons.

> **Credits:** CYD hardware and docs at [witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display). Firmware based on [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) by [artanergin44-collab](https://github.com/artanergin44-collab/cyd-gb), adapted for this monorepo with **minigb_apu** audio (internal DAC, GPIO 26).

## What you need

| Item | Notes |
|------|-------|
| ESP32-2432S028R (CYD 2.8″) | ILI9341 + XPT2046 touch |
| microSD card | **FAT32** |
| USB cable | Data + power for flash/monitor |

No extra hardware required — the CYD board already includes display, touch, and SD slot.

## SD card layout

Format as **FAT32** and create:

```
SD/
├── roms/
│   ├── gb/          ← .gb files
│   └── gbc/         ← .gbc files
├── saves/           ← battery saves (created automatically)
└── config/          ← calibration and preferences (created automatically)
```

## Features

- **On-screen controls** — D-pad, A, B, Start, Select, and pause menu
- **ROM browser** — tap to pick `.gb` / `.gbc` files
- **20 color palettes** — Classic Green, DMG, Neon, Sepia, and more
- **SD saves** — battery-backed RAM persists between sessions
- **Touch calibration** — 5-point, saved to `/config/cyd-gb.cfg`
- **SPIFFS cache** — ROM copied from SD to internal flash (faster reads)
- **Audio** — onboard amplifier output (GPIO 26, ~22 kHz)

## Controls

| Button | Location |
|--------|----------|
| D-pad | Bottom-left |
| A / B | Bottom-right |
| Select / Start | Bottom-center |
| Menu (pause) | **II** icon at top |

**Pause menu:** resume, save, load, settings, calibrate, quit.

**Settings:** palette, frame skip (0–4), brightness.

## Build and flash

From the monorepo root:

```bash
yarn cyd-gb:build
yarn cyd-gb:flash      # build + upload
yarn cyd-gb:monitor    # serial @ 115200
```

First install (erases flash and recreates SPIFFS partition):

```bash
yarn cyd-gb:install
```

Inside this directory:

```bash
yarn fw:build
yarn fw:flash
yarn fw:monitor
```

Adjust the serial port in `firmware/platformio.ini` (`upload_port` / `monitor_port`) if needed. On Linux use `/dev/ttyUSB0`; on Windows `COM3`; on macOS `/dev/tty.usbserial-*`.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Black screen | Switch `-DILI9341_2_DRIVER` to `-DILI9341_DRIVER` in `firmware/platformio.ini` |
| Imprecise touch | Pause menu → **Calibrate** |
| SPIFFS mount failed | Run `yarn cyd-gb:install` (erase + flash) |
| SD Card Error | Insert FAT32 card; reset |
| No audio | Check onboard jumper/speaker; GPIO 26 must not conflict with touch CLK (25) |

## Project layout

```
cyd-gb/
├── README.md
├── package.json          # fw:* scripts
├── scripts/pio.sh        # PlatformIO wrapper
└── firmware/
    ├── platformio.ini
    ├── partitions.csv
    ├── boards/
    ├── include/          # headers + peanut_gb.h + minigb_apu.h
    └── src/
```

## Credits and licenses

| Project | Author | License |
|---------|--------|---------|
| [**ESP32-Cheap-Yellow-Display**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) | [witnessmenow](https://github.com/witnessmenow) | MIT |
| [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) | [artanergin44-collab](https://github.com/artanergin44-collab) | MIT |
| [Peanut-GB](https://github.com/deltabeard/Peanut-GB) | Mahyar Koshkouei | MIT |
| [minigb_apu](https://github.com/minigb/minigb_apu) | — | MIT |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Bodmer | — |
| [XPT2046 Bitbang](https://github.com/TheNitek/XPT2046_Bitbang_Arduino_Library) | TheNitek | — |

Adaptations in this repo: `firmware/` layout + Yarn scripts, APU audio, `peanut_gb.h` included in tree.

## License

[MIT](../../../LICENSE) — Copyright (c) 2026 Luiz Eduardo. Upstream [cyd-gb](https://github.com/artanergin44-collab/cyd-gb) is also MIT.
