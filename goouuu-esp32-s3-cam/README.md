# GOOUUU ESP32-S3-CAM

Firmware para a placa **GOOUUU ESP32-S3-CAM** (ESP32-S3 + câmera OV2640 no módulo).

## Projetos

| Projeto | Descrição | Docs |
|---------|-----------|------|
| [**mini-games/**](mini-games/) | 12 jogos arcade nativos — OLED 128×64 + 5 botões (sem câmera, SD, Wi-Fi ou áudio) | [README](mini-games/README.md) |

## Requisitos

- [PlatformIO](https://platformio.org/)
- [Yarn](https://yarnpkg.com/) 1.x
- OLED SSD1306 I2C (3.3 V) + 5 botões táteis

## Quick start

Na raiz do monorepo:

```bash
yarn mini-games:setup    # Linux, uma vez — udev
yarn mini-games:build
yarn mini-games:flash
yarn mini-games:monitor
```

Ou dentro do projeto:

```bash
cd mini-games
yarn fw:setup
yarn fw:flash
yarn fw:monitor
```

Serial via USB CDC nativo (GPIO 19/20). Ver [mini-games/README.md](mini-games/README.md) para fiação e alimentação.
