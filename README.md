# esp-projects

Public firmware monorepo for **ESP32** boards: [github.com/LuizEduPP/esp-projects](https://github.com/LuizEduPP/esp-projects)

Each board has its own folder; each app inside a board is a self-contained [PlatformIO](https://platformio.org/) project with Yarn scripts for build and flash.

```bash
git clone https://github.com/LuizEduPP/esp-projects.git
cd esp-projects
```

## Projects

| Board | Project | Description | Docs |
|-------|---------|-------------|------|
| GOOUUU ESP32-S3-CAM | [**mini-games**](goouuu-esp32-s3-cam/mini-games/) | 12 arcade games on a 128×64 OLED + 5 buttons | [README](goouuu-esp32-s3-cam/mini-games/README.md) |
| ESP32-2432S028 (CYD) | [**cyd-gb**](esp32-cyd/cyd-gb/) | Game Boy / GBC emulator — touch controls, SD ROMs, saves ([upstream](https://github.com/artanergin44-collab/cyd-gb)) | [README](esp32-cyd/cyd-gb/README.md) |

Board index: [goouuu-esp32-s3-cam/](goouuu-esp32-s3-cam/) · [esp32-cyd/](esp32-cyd/) ([CYD docs](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display))

## Requirements

- [PlatformIO](https://platformio.org/) — IDE extension or CLI (`pio` on `PATH`)
- [Yarn](https://yarnpkg.com/) 1.x
- Linux: `dialout` group + udev rules (see setup below)
- USB cable with data lines (native CDC on supported boards)

## Quick start

From the repository root:

```bash
# Linux only, once — serial port permissions (mini-games)
yarn mini-games:setup

# build, flash, and open serial monitor
yarn mini-games:build
yarn mini-games:flash
yarn mini-games:monitor
```

CYD Game Boy emulator (based on [artanergin44-collab/cyd-gb](https://github.com/artanergin44-collab/cyd-gb)):

```bash
yarn cyd-gb:build
yarn cyd-gb:flash
yarn cyd-gb:monitor
```

Prepare a **FAT32** SD card with `roms/gb/` and `roms/gbc/` — see [cyd-gb/README.md](esp32-cyd/cyd-gb/README.md).

Or work inside the project directory:

```bash
cd goouuu-esp32-s3-cam/mini-games
yarn fw:setup    # Linux, once
yarn fw:flash
yarn fw:monitor
```

## Root scripts

Shortcuts that delegate to project `package.json` files:

| Script | Action |
|--------|--------|
| `yarn mini-games:setup` | Install udev rules (Linux) |
| `yarn mini-games:build` | Compile mini-games firmware |
| `yarn mini-games:flash` | Build + upload mini-games |
| `yarn mini-games:monitor` | Serial monitor @ 115200 |
| `yarn cyd-gb:build` | Compile cyd-gb firmware |
| `yarn cyd-gb:flash` | Build + upload cyd-gb |
| `yarn cyd-gb:install` | Erase + upload cyd-gb |
| `yarn cyd-gb:monitor` | Serial monitor @ 115200 |

## Repository layout

```
esp-projects/
├── README.md
├── package.json              # root shortcuts
└── <board-id>/               # e.g. goouuu-esp32-s3-cam, esp32-cyd
    ├── README.md             # board overview
    └── <project-name>/       # e.g. mini-games, cyd-gb
        ├── README.md         # project docs
        ├── package.json      # fw:* scripts
        ├── scripts/          # pio wrapper, udev setup
        └── firmware/         # PlatformIO project
```

## Adding a project

1. Create `<board-id>/<project-name>/` with `firmware/`, `scripts/`, and `package.json` (copy from an existing project).
2. Add a row to the **Projects** table in this README.
3. Optionally add a root shortcut in `package.json`:

```json
"my-project:flash": "yarn --cwd <board-id>/<project-name> fw:flash"
```

## Contributing

Issues and pull requests are welcome on [GitHub](https://github.com/LuizEduPP/esp-projects/issues).

## Credits

- **CYD hardware** — witnessmenow/ESP32-Cheap-Yellow-Display (MIT). Pin map, setup, and community docs for the Cheap Yellow Display.
- **cyd-gb** — derived from artanergin44-collab/cyd-gb (MIT). Emulator core: Peanut-GB.

## License

[MIT](LICENSE) — Copyright (c) 2026 Luiz Eduardo
