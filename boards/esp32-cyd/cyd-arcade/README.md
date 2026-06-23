# CYD-ARCADE

Casual games (**Snake**, **Flappy**, **Arkanoid**, **Tetris**) for the [**ESP32-2432S028R**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) (CYD). **TFT_eSPI** launcher in the style of [cyd-gb](../cyd-gb/). **No SD card** — everything is embedded in firmware.

## Requirements

| Item | Required? |
|------|-----------|
| CYD 2.8″ board | Yes |
| USB cable | Yes |
| microSD | **No** |

## Included games

| Game | Control |
|------|---------|
| **Snake** | Touch direction in the play area |
| **Flappy** | Touch screen = flap |
| **Arkanoid** | Drag horizontally |
| **Tetris** | Drag ↔ move, ↑ rotate, ↓ drop |

**Pause:** top-right corner. On **first boot** opens touch calibration. Later, **⚙️** in the header recalibrates.

## Build and flash

```bash
yarn cyd-arcade:flash
yarn cyd-arcade:monitor
```

## Adding games

Edit `firmware/src/game_catalog.cpp` and rebuild.

Supported engines: `snake`, `flappy`, `breakout`, `arkanoid`, `tetris`.

## Architecture

- **Display:** TFT_eSPI direct (like cyd-gb) — no LVGL, no 142 KB framebuffer
- **Touch:** XPT2046 on VSPI
- **Games:** incremental `tft.fillRect` / `fillCircle` in the play area

## Credits

- [CYD hardware](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — witnessmenow
- Layout inspired by **cyd-gb**
