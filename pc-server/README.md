# pc-server — Edge Pair

Servidor LAN: health, config, UI state, **processamento de câmera** e OTA de firmware.

## Setup

```bash
yarn setup
```

Edite `SERVER_URL` nos firmwares (`secrets.h`) para apontar ao IP desta máquina, ex.: `http://192.168.1.50:18124`.

## Rodar

```bash
yarn dev
```

## Endpoints

| Método | Path | Quem chama | Descrição |
|--------|------|------------|-----------|
| `POST` | `/api/v1/health` | S3 + CYD | Heartbeat e status do par |
| `GET` | `/api/v1/health/recent` | debug | Últimos heartbeats |
| `GET` | `/api/v1/config?device_id=…` | S3 + CYD | Config por dispositivo |
| `GET` | `/api/v1/ui/state?device_id=cyd-01` | CYD | Estado da tela (overlay) |
| `POST` | `/api/v1/camera/frame?device_id=…&frame_id=N` | S3 | Recebe JPEG **bruto**; gera display |
| `GET` | `/api/v1/camera/frame/info?since=N` | CYD | Metadados leves (`changed`, tamanhos) |
| `GET` | `/api/v1/camera/frame/display?since=N` | CYD | JPEG 320×201 processado |
| `GET` | `/api/v1/camera/frame/latest?since=N` | CYD | Alias de `/display` |
| `GET` | `/api/v1/firmware/manifest` | S3 + CYD | Versões OTA |
| `GET` | `/api/v1/firmware/check?role=…&version=…` | S3 + CYD | Checagem OTA |
| `GET` | `/api/v1/firmware/{role}/firmware.bin` | S3 + CYD | Binário OTA |
| `GET` | `/docs` | — | Swagger UI |

### Pipeline de câmera (contrato)

**Todo** tratamento de imagem fica no servidor — crop, resize, nitidez e **compressão para display**.

1. S3 envia JPEG **bruto** do sensor OV2640 QVGA (~9 KB) — sem pipeline de display no firmware
2. Server: crop → resize (se necessário) → UnsharpMask → re-encode JPEG display q82 4:2:2 (320×201)
3. CYD baixa `/display` (~6–8 KB) e blit **1:1** na TFT — sem resize no dispositivo

## Teste manual

```bash
# Health
curl -X POST http://localhost:18124/api/v1/health \
  -H 'Content-Type: application/json' \
  -d '{"device_id":"s3-cam-01","role":"s3","uptime_sec":10,"pair_ready":true}'

# Info do frame (poll leve)
curl 'http://localhost:18124/api/v1/camera/frame/info?device_id=s3-cam-01&since=0'

# Display JPEG
curl -o /tmp/display.jpg \
  'http://localhost:18124/api/v1/camera/frame/display?device_id=s3-cam-01&since=0'
```
