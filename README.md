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
| GOOUUU ESP32-S3-CAM | [**mini-games**](goouuu-esp32-s3-cam/mini-games/) | 12 arcade games on a 128×64 OLED + 5 buttons | [README](goouuu-esp32-s3-cam/mini-games/README.md) · [Hardware](goouuu-esp32-s3-cam/mini-games/HARDWARE.md) |

Board index: [goouuu-esp32-s3-cam/](goouuu-esp32-s3-cam/)

## Requirements

- [PlatformIO](https://platformio.org/) — IDE extension or CLI (`pio` on `PATH`)
- [Yarn](https://yarnpkg.com/) 1.x
- Linux: `dialout` group + udev rules (see setup below)
- USB cable with data lines (native CDC on supported boards)

## Quick start

From the repository root:

```bash
# Linux only, once — serial port permissions
yarn mini-games:setup

# build, flash, and open serial monitor
yarn mini-games:build
yarn mini-games:flash
yarn mini-games:monitor
```

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
| `yarn mini-games:build` | Compile firmware |
| `yarn mini-games:flash` | Build + upload |
| `yarn mini-games:monitor` | Serial monitor @ 115200 |

## Repository layout

```
esp-projects/
├── README.md
├── package.json              # root shortcuts
└── <board-id>/               # e.g. goouuu-esp32-s3-cam
    ├── README.md             # board overview
    └── <project-name>/       # e.g. mini-games
        ├── README.md         # project docs
        ├── HARDWARE.md       # wiring (optional)
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

## License

[MIT](LICENSE) — Copyright (c) 2026 Luiz Eduardo
