# CYD-GB

Emulador de **Game Boy / Game Boy Color** para o **ESP32-2432S028R** (Cheap Yellow Display). Controles 100% por touchscreen — sem botões físicos extras.

> **Créditos:** este projeto é baseado em [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) de [artanergin44-collab](https://github.com/artanergin44-collab/cyd-gb). O firmware original foi adaptado para o layout deste monorepo e recebeu suporte a áudio via **minigb_apu** (DAC interno, GPIO 26).

## O que você precisa

| Item | Notas |
|------|-------|
| ESP32-2432S028R (CYD 2.8″) | ILI9341 + touch XPT2046 |
| Cartão microSD | **FAT32** |
| Cabo USB | Dados + alimentação para flash/monitor |

Nenhum hardware adicional é necessário — a placa CYD já traz display, touch e slot SD.

## Estrutura do cartão SD

Formate em **FAT32** e crie:

```
SD/
├── roms/
│   ├── gb/          ← arquivos .gb
│   └── gbc/         ← arquivos .gbc
├── saves/           ← saves de bateria (criado automaticamente)
└── config/          ← calibração e preferências (criado automaticamente)
```

## Recursos

- **Controles na tela** — D-pad, A, B, Start, Select e menu de pausa
- **Browser de ROMs** — toque para escolher `.gb` / `.gbc`
- **20 paletas de cores** — Classic Green, DMG, Neon, Sepia e outras
- **Saves no SD** — estado de bateria persiste entre sessões
- **Calibração touch** — 5 pontos, salva em `/config/cyd-gb.cfg`
- **Cache SPIFFS** — ROM copiada do SD para flash interna (leituras mais rápidas)
- **Áudio** — saída pelo amplificador onboard (GPIO 26, ~22 kHz)

## Controles

| Botão | Posição |
|-------|---------|
| D-pad | Canto inferior esquerdo |
| A / B | Canto inferior direito |
| Select / Start | Centro inferior |
| Menu (pausa) | Ícone **II** no topo |

**Menu de pausa:** continuar, salvar, carregar, configurações, calibrar, sair.

**Configurações:** paleta, frame skip (0–4), brilho.

## Build e flash

Na raiz do monorepo:

```bash
yarn cyd-gb:build
yarn cyd-gb:flash      # build + upload
yarn cyd-gb:monitor    # serial @ 115200
```

Primeira instalação (apaga flash e recria partição SPIFFS):

```bash
yarn cyd-gb:install
```

Dentro deste diretório:

```bash
yarn fw:build
yarn fw:flash
yarn fw:monitor
```

Ajuste a porta serial em `firmware/platformio.ini` (`upload_port` / `monitor_port`) se necessário. No Linux, use `/dev/ttyUSB0`; no Windows, `COM3`; no macOS, `/dev/tty.usbserial-*`.

## Solução de problemas

| Problema | O que fazer |
|----------|-------------|
| Tela preta | Troque `-DILI9341_2_DRIVER` por `-DILI9341_DRIVER` em `firmware/platformio.ini` |
| Touch impreciso | Menu de pausa → **Calibrar** |
| Erro de SPIFFS | `yarn cyd-gb:install` (erase + flash) |
| SD Card Error | Cartão FAT32 inserido; reinicie |
| Sem áudio | Verifique jumper/speaker onboard; GPIO 26 não pode conflitar com touch CLK (25) |

## Layout do projeto

```
cyd-gb/
├── README.md
├── package.json          # scripts fw:*
├── scripts/pio.sh        # wrapper PlatformIO
└── firmware/
    ├── platformio.ini
    ├── partitions.csv
    ├── boards/
    ├── include/          # headers + peanut_gb.h + minigb_apu.h
    └── src/
```

## Créditos e licenças

| Projeto | Autor | Licença |
|---------|-------|---------|
| [**cyd-gb**](https://github.com/artanergin44-collab/cyd-gb) | [artanergin44-collab](https://github.com/artanergin44-collab) | MIT |
| [Peanut-GB](https://github.com/deltabeard/Peanut-GB) | Mahyar Koshkouei | MIT |
| [minigb_apu](https://github.com/minigb/minigb_apu) | — | MIT |
| [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Bodmer | — |
| [XPT2046 Bitbang](https://github.com/TheNitek/XPT2046_Bitbang_Arduino_Library) | TheNitek | — |

Adaptações neste repositório: estrutura `firmware/` + scripts Yarn, áudio APU, `peanut_gb.h` incluído no tree.

## Licença

[MIT](../../../LICENSE) — Copyright (c) 2026 Luiz Eduardo. O projeto upstream [cyd-gb](https://github.com/artanergin44-collab/cyd-gb) também é MIT.
