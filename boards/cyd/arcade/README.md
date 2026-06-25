# Arcade

> 12 touch games for the Cheap Yellow Display. No SD card.

## Commands

```bash
yarn arcade:flash
yarn arcade:monitor
```

## Games

Snake · Arkanoid · Tetris · Pong · Dodge · Simon · Minesweeper · Velha · Memoria · Ballz · Sandbox · Paint

First boot: touch calibration. Later: **⚙️** in header.

## Files

```
arcade/
├── README.md
├── package.json
├── scripts/          bmp_to_rgb565.py
└── firmware/
    └── src/games/
```

Layout inspired by [gb](../gb/).

## Credits

- [CYD hardware](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — witnessmenow
