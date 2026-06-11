# UART Peer — GOOUUU (edge-s3)

OV2640 JPEG bruto + upload HTTP + UART para coordenação com a CYD.

## Papel no contrato

| Faz | Não faz |
|-----|---------|
| Captura JPEG bruto OV2640 (qualidade máxima do sensor) | Crop, resize, nitidez, compressão display |
| `POST /api/v1/camera/frame` no pc-server | Enviar JPEG na UART |
| Responde `CONFIG`, `STREAM`, `CAPTURE` via UART | UI / TFT / re-encode |

**Todo** tratamento de imagem (incluindo compactar para a TFT) é feito pelo **pc-server** (orientação, suavização, crop, compressão).
No firmware S3 só há captura bruta + upload — sem `vflip`/`hmirror` no sensor.

## Wi-Fi

Credenciais vêm da **CYD** via UART (`CONFIG:wifi_*`) ou fallback `secrets.h` / NVS.

```
CONFIG:wifi_ssid=...
CONFIG:wifi_pass=...
CONFIG:server_url=http://IP:18124
```

## Câmera

- `FRAMESIZE_QVGA` (320×240), JPEG hardware OV2640, qualidade **4** (máx. do encoder)
- Monitor USB: `[cam] JPEG bruto OV2640 -> server (sem tratamento display)`

## Flash e monitor

```bash
yarn fw:flash
yarn fw:monitor
```

## Fluxo esperado

```
[cam] OV2640 QVGA bruto q4 -> server
[peer] << STREAM:ON
[cam] raw 320x240 ~9400 B
[cam] upload -> 200 {"ok":true,...}
[peer] >> FRAME:1,ok
```

Captura manual (`CAPTURE` da CYD) segue o mesmo caminho HTTP + `FRAME:id,ok|err`.

UART peer: **460800** baud (`PEER_UART_BAUD`). USB debug: 115200.

## Build

```bash
yarn fw:build
```
