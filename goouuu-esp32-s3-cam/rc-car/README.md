# RC Car — seguidor IA (GOOUUU ESP32-S3-CAM)

## Pinagem

### Motores (2× L9110S)

| GPIO | Esquerda | Direita |
|------|----------|---------|
| 1 | IA1 | |
| 2 | IB1 | |
| 14 | IA2 | |
| 47 | IB2 | |
| 21 | | IA1 |
| **48** | | IB1 + LED placa (GPIO digital) |
| 19 | | IA2 |
| 20 | | IB2 |

LEDC canal **1** = XCLK câmera. Motores usam canais 0,2,3,4,5,6,7 (definido em `pins.h`).

### Câmera OV2640

GPIO 4,5,6,7,8,9,10,11,12,13,15,16,17,18 — ver `firmware/include/pins.h`.

## Uso

```bash
yarn rc-car:upload
yarn rc-car:ai
```

Painel: **http://localhost:8765** — imagem ao vivo, cruz no centro detectado, confiança, nota do modelo.

## Como a detecção funciona

1. Modelo vision responde: `person`, `confidence`, `cx`, `cy`, `note`
2. Script **calcula motores** a partir de `cx` (não confia em left/right do modelo)
3. **Histerese**: precisa 2 frames seguidos com confiança ≥75% para "SEGUINDO"; 4 frames sem pessoa para parar
4. Badge **SEGUINDO** ≠ modelo disse person — só ativa após confirmação

## Variáveis

| Variável | Padrão | Descrição |
|----------|--------|-----------|
| `ESP_URL` | `http://192.168.1.101` | IP do ESP32 |
| `LM_MODEL` | `mistralai/ministral-3-3b` | Modelo LM Studio |
| `MIN_CONFIDENCE` | `0.75` | Limiar de confiança |
| `PERSON_ON_FRAMES` | `2` | Frames para confirmar pessoa |
| `PERSON_OFF_FRAMES` | `4` | Frames para soltar |
| `VIEW_PORT` | `8765` | Painel web |

## Teste motor manual

```bash
curl -X POST http://IP/control -H 'Content-Type: application/json' -d '{"left":180,"right":180}'
```
