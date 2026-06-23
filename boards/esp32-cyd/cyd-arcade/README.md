# CYD-ARCADE

Jogos casuais (**Snake**, **Flappy**, **Arkanoid**, **Tetris**) para o [**ESP32-2432S028R**](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) (CYD). Launcher **TFT_eSPI** no estilo do [cyd-gb](../cyd-gb/). **Sem cartão SD** — tudo embutido no firmware.

## O que precisa

| Item | Obrigatório? |
|------|----------------|
| Placa CYD 2.8″ | Sim |
| Cabo USB | Sim |
| microSD | **Não** |

## Jogos inclusos

| Jogo | Controle |
|------|----------|
| **Snake** | Toque na direção na área de jogo |
| **Flappy** | Toque na tela = bater asas |
| **Arkanoid** | Arraste o dedo na horizontal |
| **Tetris** | Arraste ↔ mover, ↑ girar, ↓ cair |

**Pausa:** canto superior direito. Na **primeira execução** abre calibração de touch. Depois, **⚙️** no header recalibra.

## Build e flash

```bash
yarn cyd-arcade:flash
yarn cyd-arcade:monitor
```

## Adicionar jogos

Edite `firmware/src/game_catalog.cpp` e recompile.

Motores suportados: `snake`, `flappy`, `breakout`, `arkanoid`, `tetris`.

## Arquitetura

- **Display:** TFT_eSPI direto (como cyd-gb) — sem LVGL, sem framebuffer de 142 KB
- **Touch:** XPT2046 no VSPI
- **Jogos:** `tft.fillRect` / `fillCircle` incremental na área de jogo

## Créditos

- [CYD hardware](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display) — witnessmenow
- Layout inspirado no **cyd-gb**
