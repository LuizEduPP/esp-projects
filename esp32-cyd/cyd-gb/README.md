# CYD-GB

**Game Boy / Game Boy Color** emulator for the [**ESP32-2432S028R**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) (Cheap Yellow Display). Fully touchscreen controlled — no extra physical buttons.

> **Credits:** hardware at [witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display). Emulator core forked from [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) by [artanergin44-collab](https://github.com/artanergin44-collab/cyd-gb), with **minigb_apu** audio (internal DAC, GPIO 26).

## What you need

| Item | Notes |
|------|-------|
| ESP32-2432S028R (CYD 2.8″) | ILI9341 240×320 + XPT2046 touch |
| microSD card | **FAT32** |
| USB cable | Data + power for flash/monitor |

No extra hardware required — the CYD board already includes display, touch, and SD slot.

## SD card layout

Format as **FAT32**:

```
SD/
├── roms/
│   ├── gb/              ← .gb files
│   ├── gb/covers/       ← BMP covers (optional)
│   ├── gbc/             ← .gbc files
│   └── gbc/covers/      ← BMP covers (optional)
├── saves/               ← battery saves (created automatically)
└── config/
    └── cyd-gb.cfg       ← preferences and calibration (created on save)
```

### ROM covers

- Format: **24-bit BMP** (up to 512×512 px)
- Filename: same as the ROM **without extension** (e.g. `Tetris (World) (Rev 1).bmp` for `Tetris (World) (Rev 1).gb`)
- Fallback: colored tile with cartridge icon when no cover is found

## Features

### Emulation

- **Peanut-GB** — `.gb` and `.gbc` (including SGB-enhanced titles in compatible mode)
- **Audio** ~22 kHz on the onboard amplifier (GPIO 26)
- **SRAM** persisted under `/saves/` on SD
- **SPIFFS cache** — ROM copied from SD to internal flash when space allows; otherwise paged reads from SD

### Interface

- Layout aligned with `mock/ui-wireframe.svg`
- **2×2 grid launcher** with pagination, covers, and palette swatches in the header
- **Virtual analog stick** (bottom-left) — drag for direction with visual knob feedback
- **A / B** buttons (matching colors, A/B layout), **Select / Start** icon pills, **Pause** in the top bar
- **Adaptive theme** — UI colors derived from the active GB palette (`ui_theme.cpp`)
- **Vector icons** rasterized to 32×32 with bilinear blit and theme tinting
- **i18n** — Portuguese, English, Spanish
- **Partial redraw** — loading, settings, stick, and FPS without full-screen flicker

### Palettes (52)

| Group | Count | Examples |
|-------|-------|----------|
| Mono reference | 2 | Classic Green, Original DMG |
| Mono-hue | 21 | Sepia, Ocean, Lava, Mint, Blood Moon… |
| Multicolor | 29 | Rainbow, Mario, Zelda, Vaporwave, Neon… |

Multicolor palettes map the four GB shades to distinct hues. Change under **Settings → Palette**; preview shows four swatches.

### Persistent config (`/config/cyd-gb.cfg`)

| Key | Description |
|-----|-------------|
| `pal` | Palette index (0–51) |
| `fskip` | Frame skip (0–4) |
| `bright` | Backlight level (0–255) |
| `lang` | Language (0=PT, 1=EN, 2=ES) |
| `cal`, `xmin`, `xmax`, `ymin`, `ymax` | Touch calibration (5-point) |

Saved when confirming Settings or completing calibration.

## Controls

| Control | Location |
|---------|----------|
| Stick / D-pad | Bottom-left |
| A / B | Bottom-right |
| Select / Start | Bottom-center |
| Menu (pause) | **II** icon at top |

**In-game pause menu:** resume, save, load, settings, calibrate, quit.

**Settings (launcher or pause):** palette, frame skip, brightness, language.

**Calibration:** 5 points (four corners + center) with linear regression; available from pause menu, boot, or when SD fails.

## Build and flash

From the monorepo root:

```bash
yarn cyd-gb:build
yarn cyd-gb:flash      # build + upload
yarn cyd-gb:monitor    # serial @ 115200
yarn cyd-gb:icons      # regenerate icons from SVG
```

First install (erases flash and recreates partitions):

```bash
yarn cyd-gb:install
```

Inside this directory:

```bash
yarn fw:build
yarn fw:flash
yarn fw:monitor
yarn icons
```

Adjust the serial port in `firmware/platformio.ini` (`upload_port` / `monitor_port`) if needed. Linux: `/dev/ttyUSB0`; Windows: `COM3`; macOS: `/dev/tty.usbserial-*`.

### Regenerate UI icons

Icons are sourced from `mock/ui-icons-sheet.svg`. Requires Python 3 with `cairosvg` and `Pillow`:

```bash
yarn cyd-gb:icons
```

Outputs PNGs to `firmware/assets/icons/` and the alpha mask to `firmware/include/ui_icon_data.h`.

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Black screen | Switch `-DILI9341_2_DRIVER` to `-DILI9341_DRIVER` in `firmware/platformio.ini` |
| Imprecise touch | Pause menu → **Calibrate** |
| SPIFFS mount failed | Run `yarn cyd-gb:install` (erase + flash) |
| SD Card Error | Insert FAT32 card; reset |
| No audio | Check onboard jumper/speaker; GPIO 26 must not conflict with touch CLK (25) |
| Cover not shown | 24-bit BMP; filename matches ROM; correct `covers/` folder (gb vs gbc) |

## Project layout

```
cyd-gb/
├── README.md
├── package.json              # fw:* and icons scripts
├── mock/
│   ├── ui-wireframe.svg      # layout reference
│   └── ui-icons-sheet.svg    # icon sprites
├── scripts/
│   ├── pio.sh                # PlatformIO wrapper
│   └── gen_ui_icons.py       # SVG → embedded bitmap
└── firmware/
    ├── platformio.ini
    ├── partitions.csv        # 2 MB app + ~2 MB SPIFFS
    ├── boards/
    ├── assets/icons/         # generated PNGs
    ├── include/              # headers + peanut_gb.h + ui_icon_data.h
    └── src/
        ├── main.cpp
        ├── emulator_bridge.cpp
        ├── display.cpp
        ├── touch_input.cpp
        ├── sd_manager.cpp
        ├── ui_launcher.cpp
        ├── ui_theme.cpp
        ├── ui_icons.cpp
        └── ...
```

## Limitations

- **Game Boy / GBC only** — NES and other systems need a different emulator core
- Up to **64 ROMs** listed in the launcher
- Large ROMs depend on SD or free SPIFFS space for caching

## Credits and licenses

| Project | Author | License |
|---------|--------|---------|
| **This fork** (UI redesign, wireframe, icons, theme, palettes, launcher) | **Luiz Eduardo** | MIT |
| [**ESP32-Cheap-Yellow-Display**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) | witnessmenow | MIT |
| [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) (upstream emulator base) | artanergin44-collab | MIT |
| [Peanut-GB](https://github.com/deltabeard/Peanut-GB) | Mahyar Koshkouei | MIT |
| [minigb_apu](https://github.com/minigb/minigb_apu) | — | MIT |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Bodmer | — |
| [XPT2046 Bitbang](https://github.com/TheNitek/XPT2046_Bitbang_Arduino_Library) | TheNitek | — |
| libretro thumbnails | RetroArch community | box art source |

**Luiz Eduardo** — UI/UX redesign (`mock/ui-wireframe.svg`, `mock/ui-icons-sheet.svg`), adaptive theme, 52 palettes, grid launcher, BMP covers, i18n, 5-point touch calibration, analog stick, SD config persistence, Yarn/PlatformIO layout.

## License

[MIT](../../../LICENSE) — Copyright (c) 2026 Luiz Eduardo. Upstream [cyd-gb](https://github.com/artanergin44-collab/cyd-gb) is also MIT.
