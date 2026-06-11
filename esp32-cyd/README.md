# ESP32-2432S028R (CYD)

[**Cheap Yellow Display**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) board (ESP32-2432S028R) — ESP32 with **2.8″ ILI9341** TFT, **XPT2046** touch, microSD slot, and RGB LED.

Board docs and pin map: [witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display).

## Projects

| Project | Description | Docs |
|---------|-------------|------|
| [**cyd-gb/**](cyd-gb/) | Game Boy / GBC emulator with SD ROM launcher and touch controls | [README](cyd-gb/README.md) |

## Requirements

- [PlatformIO](https://platformio.org/) (CLI or IDE extension)
- [Yarn](https://yarnpkg.com/) 1.x
- **FAT32** microSD card (for cyd-gb)

## Quick start

From the monorepo root:

```bash
yarn cyd-gb:build
yarn cyd-gb:flash
yarn cyd-gb:monitor
```

Or inside the project:

```bash
cd cyd-gb
yarn fw:flash
yarn fw:monitor
```

## Credits

- **CYD hardware** — docs and community at [witnessmenow/ESP32-Cheap-Yellow-Display](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) (MIT).
- **cyd-gb** — firmware derived from [artanergin44-collab/cyd-gb](https://github.com/artanergin44-collab/cyd-gb). See [cyd-gb/README.md](cyd-gb/README.md).
