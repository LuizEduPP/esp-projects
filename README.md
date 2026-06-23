# esp-projects

Public firmware monorepo for **ESP32** boards: [github.com/LuizEduPP/esp-projects](https://github.com/LuizEduPP/esp-projects)

Each board lives under `boards/<board-id>/`; each app is a self-contained [PlatformIO](https://platformio.org/) project with Yarn scripts for build and flash.

```bash
git clone https://github.com/LuizEduPP/esp-projects.git
cd esp-projects
```

## Projects

| Board | Project | Description | Docs |
|-------|---------|-------------|------|
| GOOUUU ESP32-S3-CAM | [**mini-games**](boards/goouuu-esp32-s3-cam/mini-games/) | 12 arcade games on a 128×64 OLED + 5 buttons | [README](boards/goouuu-esp32-s3-cam/mini-games/README.md) |
| GOOUUU ESP32-S3-CAM | [**rc-car**](boards/goouuu-esp32-s3-cam/rc-car/) | 4WD AI follower — camera + LM Studio | [README](boards/goouuu-esp32-s3-cam/rc-car/README.md) |
| ESP32-2432S028 (CYD) | [**cyd-gb**](boards/esp32-cyd/cyd-gb/) | Game Boy / GBC emulator — touch controls, SD ROMs, saves ([upstream](https://github.com/artanergin44-collab/cyd-gb)) | [README](boards/esp32-cyd/cyd-gb/README.md) |
| ESP32-2432S028 (CYD) | [**cyd-arcade**](boards/esp32-cyd/cyd-arcade/) | Casual games (Snake, Flappy, Arkanoid, Tetris) — no SD required | [README](boards/esp32-cyd/cyd-arcade/README.md) |

Board index: [boards/](boards/) · [goouuu-esp32-s3-cam](boards/goouuu-esp32-s3-cam/) · [esp32-cyd](boards/esp32-cyd/) ([CYD docs](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display))

## Requirements

- [PlatformIO](https://platformio.org/) — IDE extension or CLI (`pio` on `PATH`)
- [Yarn](https://yarnpkg.com/) 1.x
- Linux: `dialout` group + udev rules (see setup below)
- USB cable with data lines (native CDC on supported boards)

## Quick start

From the repository root:

```bash
# Linux only, once — serial port permissions (all boards)
yarn setup

# mini-games: build, flash, and open serial monitor
yarn mini-games:build
yarn mini-games:flash
yarn mini-games:monitor
```

CYD Game Boy emulator:

```bash
yarn cyd-gb:build
yarn cyd-gb:flash
yarn cyd-gb:monitor
```

Prepare a **FAT32** SD card with `roms/gb/` and `roms/gbc/` — see [cyd-gb/README.md](boards/esp32-cyd/cyd-gb/README.md).

Or work inside a project directory:

```bash
cd boards/goouuu-esp32-s3-cam/mini-games
yarn fw:flash
yarn fw:monitor
```

## Root scripts

Shortcuts that delegate to workspace `package.json` files:

| Script | Action |
|--------|--------|
| `yarn setup` | Install udev rules (Linux, all boards) |
| `yarn mini-games:*` | Build / flash / monitor mini-games |
| `yarn rc-car:*` | Build / flash / monitor rc-car + AI tools |
| `yarn cyd-gb:*` | Build / flash / install / monitor cyd-gb |
| `yarn cyd-arcade:*` | Build / flash / install / monitor cyd-arcade |

## Repository layout

```
esp-projects/
├── README.md
├── package.json              # Yarn workspaces + root shortcuts
├── scripts/                  # shared tooling (pio, udev setup)
├── docs/                     # hardware notes, misc docs
└── boards/
    └── <board-id>/           # e.g. goouuu-esp32-s3-cam, esp32-cyd
        ├── README.md         # board overview
        └── <project-name>/   # e.g. mini-games, cyd-gb
            ├── README.md     # project docs
            ├── package.json  # fw:* scripts
            ├── scripts/      # project-specific tooling
            └── firmware/     # PlatformIO project
```

## Adding a project

1. Create `boards/<board-id>/<project-name>/` with `firmware/`, `scripts/` (if needed), and `package.json` (copy from an existing project).
2. Register the workspace path in root `package.json` if the board folder pattern does not match `boards/<board-id>/*`.
3. Add a row to the **Projects** table in this README.
4. Optionally add a root shortcut in `package.json`.

## Contributing

Issues and pull requests are welcome on [GitHub](https://github.com/LuizEduPP/esp-projects/issues).

## Credits

- **CYD hardware** — witnessmenow/ESP32-Cheap-Yellow-Display (MIT). Pin map, setup, and community docs for the Cheap Yellow Display.
- **cyd-gb** — derived from artanergin44-collab/cyd-gb (MIT). Emulator core: Peanut-GB.

## License

[MIT](LICENSE) — Copyright (c) 2026 Luiz Eduardo
