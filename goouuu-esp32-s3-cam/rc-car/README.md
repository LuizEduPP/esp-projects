# RC Car — seguidor de pessoa com IA (LM Studio)

Carro 4WD + camera OV2640. O **ESP32** expoe JPEG e motores via HTTP; um script no PC chama **LM Studio** com [mistralai/ministral-3-3b](https://lmstudio.ai/models/mistralai/ministral-3-3b) para decidir direcao.

## Arquitetura

```
ESP32-S3-CAM  --WiFi-->  PC (yarn rc-car:ai)  --HTTP-->  LM Studio :1234
     |                         |
  /capture JPEG          Ministral 3 3B (vision)
  /control motores       JSON {left, right}
```

## 1. Firmware (ESP32)

Edite WiFi em `firmware/platformio.ini`:

```ini
build_flags =
    ...
    -DWIFI_SSID=\"sua-rede\"
    -DWIFI_PASS=\"sua-senha\"
```

```bash
yarn rc-car:upload
yarn rc-car:monitor   # anote o IP exibido
```

Endpoints:
- `GET /capture` — JPEG da camera
- `POST /control` — `{"left":160,"right":160}` (-255..255)
- `GET /status` — IP e ultimo comando

Sem comando novo por ~1 s → motores param (fail-safe).

## 2. LM Studio (PC)

1. Baixe e carregue **[mistralai/ministral-3-3b](https://lmstudio.ai/models/mistralai/ministral-3-3b)** (vision + JSON nativo)
2. Developer → Local Server → Start (porta **1234**)

## 3. Script de seguimento + painel visual

```bash
ESP_URL=http://192.168.x.x yarn rc-car:ai
```

Abra no navegador: **http://localhost:8765** — mostra camera, deteccao (pessoa sim/nao), motores e resposta bruta do modelo.

Variaveis opcionais:

| Variavel | Padrao | Descricao |
|----------|--------|-----------|
| `ESP_URL` | `http://192.168.1.101` | IP do ESP32 (veja no serial apos upload) |
| `LM_URL` | `http://127.0.0.1:1234/v1/chat/completions` | API LM Studio |
| `LM_MODEL` | `mistralai/ministral-3-3b` | Modelo carregado |
| `INTERVAL_MS` | `800` | Pausa entre frames |
| `VIEW_PORT` | `8765` | Porta do painel web |

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

**GPIO48** = LED da placa + IB1 do motor (GPIO digital, nao PWM).

## Build

```bash
yarn rc-car:setup
yarn rc-car:build
yarn rc-car:upload
```

## Dicas

- Ministral 3 3B e mais leve que Gemma — resposta tipicamente mais rapida (~2 GB RAM)
- Latencia depende do GPU/CPU — ajuste `INTERVAL_MS` se necessario
- Teste manual: `curl http://IP/capture -o test.jpg` e `curl -X POST http://IP/control -d '{"left":0,"right":0}'`
