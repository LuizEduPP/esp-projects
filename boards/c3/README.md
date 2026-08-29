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

GPIO 2 and GPIO 8 are C3 strapping pins — both must read HIGH at reset. Holding the top-right button while resetting can stall the boot.

## Facts from the vendor schematic

[ESP32-C3-1.44inch.pdf](https://cdn.static.spotpear.com/uploads/picture/product/esp32/ESP32C3-1.44/Schematic/ESP32-C3-1.44inch.pdf) ·
[demo code](https://github.com/Spotpear/ESP32C3_1.44inch) ·
[vendor wiki](https://spotpear.com/wiki/ESP32-C3-desktop-trinket-Mini-TV-Portable-Pendant-LVGL-1.44inch-LCD-ST7735.html)

- **Flash is 16 MB** (W25Q128), not the 4 MB the `esp32-c3-devkitm-1` board definition assumes.
  Set `flash_size = 16MB` and `default_16MB.csv`, confirmed on device with `esptool flash_id`.
- **The backlight is not switchable.** The panel's LEDA/LEDK go to 3V3 through a fixed resistor —
  no GPIO in the path. `DISPOFF`/`SLPIN` blanks the pixels but the LED stays on.
- **There is a battery charger on board**: PL4054, LiPo on a 1.27 mm connector, charging over USB.
- **A user LED sits on GPIO 11**, next to the reset button.
- Free GPIOs on the header: 1, 6, 7, 20, 21.

## Upload stalls with "Write timeout"

Native USB sometimes misses the auto-reset:

1. Hold **BOOT**.
2. Tap **RESET**.
3. Release BOOT — the board is now in download mode.
4. Pick the new port and flash.
