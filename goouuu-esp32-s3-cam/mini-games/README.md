# Mini Games

Twelve native arcade games on a **128×64 SSD1306 OLED** and **5 buttons**. Runs entirely on the MCU — no SD card, network, sound, or camera.

## What you need

| Part | Role |
|------|------|
| GOOUUU ESP32-S3-CAM | Main board |
| SSD1306 OLED (I2C, 3.3 V) | Display |
| 5× tactile buttons | Input (Down, Up, Left, Right, A) |
| Jumpers + breadboard (optional) | Wiring |

Full pinout and wiring: [HARDWARE.md](HARDWARE.md).

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

```bash
yarn fw:setup    # Linux once — udev rules
yarn fw:build
yarn fw:flash
yarn fw:monitor
```

Serial uses native USB CDC (GPIO 19/20). See [HARDWARE.md](HARDWARE.md) for power and cable notes.
