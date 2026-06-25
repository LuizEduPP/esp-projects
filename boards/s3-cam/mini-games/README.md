# Mini Games

> 12 arcade games on 128×64 SSD1306 OLED + 5 buttons. No camera, Wi-Fi, or SD.

## Commands

```bash
yarn mini-games:flash
yarn mini-games:monitor
```

## Hardware

| Part | GPIO |
|------|------|
| OLED SDA / SCL | 43 / 44 |
| Buttons D U L R A | 2, 21, 14, 47, 1 |
| USB CDC | 19 / 20 |

Power OLED from **3.3 V**. Buttons: GPIO → GND (INPUT_PULLUP).

## Controls

D-pad = move · **A** = action · **Left+Right** (1.2 s) = back to menu

Games: Snake · Tetris · Memory · Pong · Breakout · Space · Flappy · Dodge · 2048 · Frog · Mines · Asteroids

## Files

```
mini-games/
├── README.md
├── package.json
└── firmware/
```
