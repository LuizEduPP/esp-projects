# Mini Games

Twelve native arcade games on a **128×64 SSD1306 OLED** and **5 buttons**. Runs entirely on the MCU — no SD card, network, sound, or camera.

## What you need

| Part | Role |
|------|------|
| GOOUUU ESP32-S3-CAM | Main board |
| SSD1306 OLED (I2C, 3.3 V) | Display |
| 5× tactile buttons | Input (Down, Up, Left, Right, A) |
| Jumpers + breadboard (optional) | Wiring |

The on-board camera, microSD slot, Wi-Fi, and audio are not used.

## Pin summary

| Signal | GPIO |
|--------|------|
| OLED SDA | 43 |
| OLED SCL | 44 |
| Btn Down | 2 |
| Btn Up | 21 |
| Btn Left | 14 |
| Btn Right | 47 |
| Btn A | 1 |

Native USB CDC: D+ **19**, D− **20**.

## Hardware

**Power:** feed the ESP from USB-C. Connect OLED **VDD → 3.3 V** and **GND → GND**. Do not power the OLED from 5 V.

**OLED (I2C):** SDA → GPIO 43, SCL → GPIO 44. Address is usually **0x3C** (firmware probes 0x3D too).

**Buttons:** firmware uses `INPUT_PULLUP`. Wire each GPIO to GND through a tactile button (released = HIGH, pressed = LOW). Suggested order: Down · Up · Left · Right · A.

**GPIO to avoid:** 4–18, 8–13, 15, 16 (camera); 35–42, 45 (SD/PSRAM); 48 (RGB LED); 0, 3, 46 (strapping).

**Bench supply:** if the ESP is powered from the **5 V** header, do not connect USB VBUS at the same time — use a data-only USB-C cable for flash/monitor.

## Controls

| Input | Action |
|-------|--------|
| D-pad | Move / navigate (hold to repeat) |
| **A** | Action / confirm / retry |
| **Left + Right** (hold ~1.2 s) | Back to menu |

## Games

Snake · Tetris · Memory · Pong · Breakout · Space · Flappy · Dodge · 2048 · Frog · Mines · Asteroids

Best scores are saved in NVS flash.

## Build and flash

From the monorepo root:

```bash
yarn mini-games:setup    # Linux once — udev rules
yarn mini-games:build
yarn mini-games:flash
yarn mini-games:monitor
```

Or inside this directory:

```bash
yarn fw:setup    # Linux once — udev rules
yarn fw:build
yarn fw:flash
yarn fw:monitor
```

Default serial port in `firmware/platformio.ini`: `/dev/ttyACM0` (native USB CDC @ 115200).
