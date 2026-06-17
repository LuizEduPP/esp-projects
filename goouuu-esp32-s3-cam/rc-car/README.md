# RC Car — GOOUUU ESP32-S3-CAM

4WD com 2× L9110S. Controle via **[BLE Controller](https://play.google.com/store/apps/details?id=com.circuitmagic.iot)** (CircuitMagic) — **sem configurar botoes**.

## Uso

1. `yarn rc-car:upload`
2. Abrir **BLE Controller** no celular
3. Scan → conectar em **`GOOUUU-RC`**
4. Usar o controle/joystick padrao do app

O firmware aceita automaticamente:

- Botoes padrao **F / B / L / R / S**
- **W A S D** e setas
- Teclado numerico **1–4** (mover), **0** (parar)
- **Joystick** (valores X,Y ou esquerda,direita)

## Fiação motores

| GPIO | L9110S #1 (esq) | L9110S #2 (dir) |
|------|-----------------|-----------------|
| 1 | IA1 | |
| 2 | IB1 | |
| 14 | IA2 | |
| 47 | IB2 | |
| 21 | | IA1 |
| 48 | | IB1 |
| 19 | | IA2 |
| 20 | | IB2 |

Motores: 4× AA (~6 V). **GND comum.**

## Build

```bash
yarn rc-car:setup
yarn rc-car:build
yarn rc-car:upload
```
