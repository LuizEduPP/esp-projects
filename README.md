# esp-projects

Firmware monorepo for ESP32 boards — [github.com/LuizEduPP/esp-projects](https://github.com/LuizEduPP/esp-projects)

```bash
git clone https://github.com/LuizEduPP/esp-projects.git && cd esp-projects
yarn install && yarn setup          # Linux: udev, once
yarn mini-games:flash               # pick any project below
```

## Projects

| Board | Folder | App | Flash |
|-------|--------|-----|-------|
| **S3-CAM** | [`boards/s3-cam/`](boards/s3-cam/) | [mini-games](boards/s3-cam/mini-games/) — OLED arcade | `yarn mini-games:flash` |
| | | [rc-car](boards/s3-cam/rc-car/) — AI follower + LM Studio | `yarn rc-car:flash` |
| | | [folio](boards/s3-cam/folio/) — mic + camera push | `yarn folio:flash` |
| **CYD** | [`boards/cyd/`](boards/cyd/) | [gb](boards/cyd/gb/) — Game Boy emulator | `yarn gb:flash` |
| | | [arcade](boards/cyd/arcade/) — 12 touch games | `yarn arcade:flash` |

Each app = `README.md` + `package.json` + `firmware/` (+ optional `scripts/`).

## Layout

```
esp-projects/
├── README.md           ← you are here
├── package.json        ← yarn shortcuts
├── scripts/            ← pio.sh, udev setup (shared)
├── docs/               ← guide + hardware list
└── boards/
    ├── s3-cam/         ← GOOUUU ESP32-S3-CAM
    │   ├── mini-games/
    │   ├── rc-car/
    │   └── folio/
    └── cyd/            ← ESP32-2432S028R (Cheap Yellow Display)
        ├── gb/
        └── arcade/
```

Details: [docs/guide.md](docs/guide.md) · Parts: [docs/hardware-inventory.md](docs/hardware-inventory.md)

## All commands

| Prefix | Build | Flash | Monitor |
|--------|-------|-------|---------|
| `mini-games:` | ✓ | ✓ | ✓ |
| `rc-car:` | ✓ | ✓ | ✓ (+ `ai`, `diag`) |
| `folio:` | ✓ | ✓ | ✓ |
| `gb:` | ✓ | ✓ | ✓ (+ `install`, `icons`) |
| `arcade:` | ✓ | ✓ | ✓ (+ `install`, `preview`) |

Inside any app folder: `yarn fw:flash` · `yarn fw:monitor`

## License

[MIT](LICENSE) — Copyright (c) 2026 Luiz Eduardo
