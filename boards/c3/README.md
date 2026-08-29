# C3

**Spotpear ESP32-C3 1.44″** (Spaceman / Pendant) — ESP32-C3 + 128×128 ST7735 TFT + 2 buttons + LiPo case.

| App | What | Flash |
|-----|------|-------|
| [dash](dash/) | Clock, date, weather and a line from local Ollama | `yarn dash:flash` |

## Pinout

The vendor hid the display SPI bus: the pins on the top header (6, 7, 20, 21) are **not** the screen.

| Signal | GPIO |
|--------|------|
| TFT_SCLK | 3 |
| TFT_MOSI | 4 |
| TFT_CS | 2 |
| TFT_DC | 0 |
| TFT_RST | 5 |
| Button NAV (top right) | 8 |
| Button SEL (top left) | 10 |
| Button BOOT | 9 |

ST7735 init profile: `INITR_144GREENTAB`.

GPIO 2 and GPIO 8 are C3 strapping pins — both must read HIGH at reset. Holding NAV while resetting can stall the boot.

## Upload stalls with "Write timeout"

Native USB sometimes misses the auto-reset:

1. Hold **BOOT**.
2. Tap **RESET**.
3. Release BOOT — the board is now in download mode.
4. Pick the new port and flash.
