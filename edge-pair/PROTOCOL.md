# Protocolo UART — Edge Pair

Bus serial entre **S3-CAM** (edge-s3) e **CYD** (edge-cyd). **Somente coordenação** — pareamento, config, stream on/off, aviso de frame. **Sem payload binário** (JPEG vai por Wi-Fi → pc-server).

Fiação: [HARDWARE.md](HARDWARE.md) · Arquitetura: [README.md](README.md)

---

## Camada física

| Item | Valor |
|------|-------|
| Baud | **460800** (`PEER_UART_BAUD` em `pins.h`) |
| Formato | 8N1 |
| Fios | TX, RX, GND |
| S3 | TX=GPIO43, RX=GPIO44 |
| CYD (uart-peer) | RX=GPIO35 (P3), TX=GPIO1 (P1) |

Monitor USB de debug permanece **115200** — é outra UART (CH340), independente do link peer.

---

## Formato das mensagens

**Somente linhas de texto** terminadas em `\n`:

```
COMANDO[:arg1][,arg2=valor]...\n
```

- ASCII printable; `\r` opcional (ignorar em `\r\n`).
- Máximo recomendado: **256 bytes** por linha.
- Args separados por `,`; pares `chave=valor` sem espaços.

**Não há frames binários** neste protocolo. Imagem bruta e display passam pelo pc-server (ver [README → API](README.md#api-do-pc-servidor)).

### Handshake no boot

```
S3 → CYD:  PAIR:HELLO,ID=s3-cam-01
CYD → S3:  PAIR:OK,ID=cyd-01
S3 → CYD:  PAIR:READY
CYD → S3:  PAIR:READY
CYD → S3:  CONFIG:wifi_ssid=...
CYD → S3:  CONFIG:wifi_pass=...
CYD → S3:  CONFIG:server_url=http://IP:18124
CYD → S3:  STREAM:ON
```

Se `PAIR:OK` não chegar em 3 s, S3 reenvia `PAIR:HELLO` (até 5 tentativas). Timeout de link: 5 s após `PAIR:OK` sem `PAIR:READY` → S3 reinicia handshake.

---

## Comandos S3 → CYD

| Linha | Descrição |
|-------|-----------|
| `PAIR:HELLO,ID=s3-cam-01` | Início do handshake |
| `PAIR:READY` | Link UART pronto |
| `FRAME:id,ok` | Upload HTTP do frame `id` concluído |
| `FRAME:id,err` | Upload HTTP falhou (Wi-Fi off, server down, etc.) |
| `HEARTBEAT` | S3 viva (~a cada 10 s com link pronto) |

---

## Comandos CYD → S3

| Linha | Descrição |
|-------|-----------|
| `PAIR:OK,ID=cyd-01` | Resposta ao hello |
| `PAIR:READY` | Confirma link bidirecional |
| `CONFIG:wifi_ssid=...` | Provisiona SSID na S3 (NVS) |
| `CONFIG:wifi_pass=...` | Provisiona senha na S3 |
| `CONFIG:server_url=...` | URL do pc-server; aplica e reconecta Wi-Fi |
| `STREAM:ON` | Habilita captura contínua (~1,4 s) na S3 |
| `STREAM:OFF` | Pausa captura contínua |
| `CAPTURE` | Uma captura imediata + upload |
| `ACK:HEARTBEAT` | Resposta ao heartbeat da S3 |

---

## Fluxo de stream (v1)

```
CYD → S3:   STREAM:ON
S3:         loop: captura QVGA JPEG bruto (OV2640)
S3 → PC:    POST /api/v1/camera/frame?device_id=s3-cam-01&frame_id=N
S3 → CYD:   FRAME:N,ok
CYD:        GET /api/v1/camera/frame/info?since=M  (poll ~120 ms)
CYD:        GET /api/v1/camera/frame/display?since=M  (se changed)
CYD:        decode JPEG + blit 1:1 na área 320×201 do TFT
```

A CYD **não** recebe JPEG pela UART e **não** faz crop/resize/compressão. O pc-server faz todo o tratamento e re-encode para display (320×201).

---

## Exemplo — captura manual

```
CYD → S3:  CAPTURE
S3 → PC:   POST /api/v1/camera/frame (JPEG bruto)
S3 → CYD:  FRAME:7,ok
CYD:       kick imediato no poll HTTP → baixa /display
```

---

## Exemplo — upload falhou

```
S3 → CYD:  FRAME:8,err
CYD:       mantém último frame na tela; poll continua
```

*(Proxy de upload pela CYD — planejado, não implementado.)*

---

## Timeout e erro

| Situação | Ação |
|----------|------|
| Sem `HEARTBEAT` 15 s | CYD pode marcar par degradado (UI futura) |
| Linha inválida | Ignorar; log serial USB |
| `FRAME:err` | CYD não avança preview; S3 tenta no próximo ciclo |
| Handshake timeout | S3 reenvia `PAIR:HELLO` |

---

## Implementação (referência)

### Parser de linha (ambos os lados)

```cpp
String line;
while (peer.available()) {
  char c = peer.read();
  if (c == '\n') {
    handleLine(line);
    line = "";
  } else if (c != '\r') {
    line += c;
  }
}
```

### S3 — notificar frame após upload

```cpp
PeerSerial.printf("FRAME:%d,%s\n", frameId, ok ? "ok" : "err");
```

### CYD — reagir a frame pronto

```cpp
if (line.startsWith("FRAME:")) {
  wifiCameraFetchKick();  // acelera poll HTTP
}
```

---

## Fora de escopo (v1)

Comandos da spec antiga **não implementados** — reservados para versões futuras:

- `EVT:…`, `THUMB` + binário, `ACK:…,THUMB`
- `PAUSE` / `RESUME` (substituídos por `STREAM:OFF` / `STREAM:ON`)
- `STATUS:busy`, `FAIL:UPLOAD`, `SYNC:mode=…`

---

## Status

| Item | Status |
|------|--------|
| Spec UART (este doc) | ✅ v1 — texto only, 460800 |
| Firmware `uart-peer` S3 + CYD | ✅ implementado |
| Stream câmera via HTTP | ✅ pc-server + firmware |
