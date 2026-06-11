# Edge Pair — S3-CAM + CYD + PC

Arquitetura de um **par de placas ESP** que compartilham processamento na borda, com **Wi-Fi sempre ativo** nos dois nós e um **PC na rede local** como servidor (LLM, visão pesada, histórico e regras).

Este documento descreve o desenho físico, lógico e de protocolo. Firmware **`uart-peer`** (S3 + CYD) e **`pc-server`** estão implementados — ver status ao final.

## Visão geral

Dois ESP32 formam um **cluster de borda** de dois nós:

| Nó | Placa | Papel |
|----|-------|-------|
| **Visão** | GOOUUU ESP32-S3-CAM | Câmera OV2640, captura, encode, SD de mídia |
| **Interface** | ESP32-2432S028 (CYD) | Display TFT + touch, UI, SD de assets, fila HTTP |
| **Servidor** | PC na LAN | API, banco, LLM (Ollama), visão pesada, configuração do par |

```
                    ┌─────────────────────┐
                    │   PC (servidor)     │
                    │   LLM · API · DB    │
                    └──────────▲───▲──────┘
                               │   │
                      Wi-Fi    │   │    Wi-Fi (sempre)
                               │   │
              ┌────────────────┘   └────────────────┐
              │                                      │
       ┌──────┴──────┐      UART        ┌───────────┴──────┐
       │  S3-CAM     │◄────────────────►│  CYD             │
       │  edge-s3    │   bus interno    │  edge-cyd        │
       └─────────────┘                  └──────────────────┘
         SD #1 — mídia                    SD #2 — UI / fila
```

**Princípio:** o par resolve o que é **urgente e local** (alerta, preview, comandos). O PC resolve o que é **profundo** (linguagem, face pesada, relatórios, regras complexas).

---

## Hardware

### Bill of materials

| Qty | Parte | Função |
|-----|-------|--------|
| 1 | GOOUUU ESP32-S3-CAM | Câmera + PSRAM + slot microSD |
| 1 | ESP32-2432S028 (CYD) | TFT 320×240 + touch + slot microSD |
| 2 | Cartão microSD | Um por placa (papéis distintos) |
| — | Jumpers | UART (TX/RX) + barramento 5 V/GND compartilhado |
| 1 | Fonte **5 V ≥ 2 A** | **Um único** ponto (USB, bench ou **MB102**) |
| 1 | MB102 + breadboard *(opc.)* | Distribui 5 V — ver [HARDWARE.md](HARDWARE.md#opção-d--mb102-usb-type-a--5-v--33-v--breadboard) |
| 1 | PC na mesma LAN | Servidor com Ollama ou API equivalente |

### Ligação UART (resumo)

| Fio | S3-CAM (GOOUUU) | CYD (2432S028) |
|-----|-----------------|----------------|
| TX → RX | **TX0** GPIO **43** | **P3 GPIO 35** |
| RX ← TX | **RX0** GPIO **44** | **P1 TX** (GPIO 1) |
| 5 V | header **5V** | P1 **VIN** |
| GND | GND | P1 **GND** |

P1 **RX (GPIO 3) sem fio** — conflito CH340. Detalhes: [HARDWARE.md → híbrido](HARDWARE.md#p1p3-híbrido-uart-peer--serial).

Baud **460800** (`PEER_UART_BAUD`), 8N1, lógica **3,3 V**, TX/RX **cruzados**. Monitor USB de debug: 115200 (UART separada).

**Energia:** um único ponto (USB ou fonte 5 V ≥ 2 A) alimenta as duas placas — ver [**HARDWARE.md → Alimentação**](HARDWARE.md#alimentação-única).

Guia completo: [**HARDWARE.md**](HARDWARE.md) · Mensagens UART: [**PROTOCOL.md**](PROTOCOL.md)

Câmera e display já vêm integrados nas PCBs — **sem solda de alta frequência** ou flat cables soltos.

---

## Divisão de armazenamento (dois SD)

Não misturar mídia bruta com assets de interface.

| SD | Dono | Conteúdo |
|----|------|----------|
| **#1** | S3-CAM | Fotos/vídeos, logs, modelos `.tflite`, fila `pending/` se upload falhar |
| **#2** | CYD | Ícones LVGL, fontes, temas, fila HTTP offline, cache de respostas do PC |

---

## Divisão de processamento

| Tarefa | S3-CAM | CYD | PC |
|--------|:------:|:---:|:--:|
| Captura OV2640 (JPEG bruto do sensor) | ✅ | — | — |
| Upload JPEG bruto | ✅ | — | recebe |
| Crop / resize / nitidez / **compressão display** | — | — | ✅ |
| Preview TFT (blit 1:1 do `/display`) | — | ✅ | serve JPEG processado |
| Coordenação UART (stream, config) | ✅ | ✅ | — |
| HTTP health / config / UI | ✅ | ✅ | ✅ |
| LLM, eventos, chat | — | — | ⬜ planejado |
| Heartbeat | ✅ | ✅ | recebe |

### Papéis fixos (v1)

- **S3** = captura JPEG bruto e upload HTTP; UART só coordenação.
- **CYD** = poll HTTP + blit na TFT; UART envia CONFIG/STREAM/CAPTURE.
- **PC** = processamento de imagem, API, OTA; LLM/eventos planejados.

---

## Wi-Fi (sempre ligado)

Wi-Fi é **obrigatório** nos dois nós — boot, reconexão automática e heartbeat periódico.

```cpp
#define WIFI_SSID       "..."
#define WIFI_PASS       "..."
#define SERVER_URL      "http://192.168.1.50:18124"
#define DEVICE_ID       "s3-cam-01"   // ou "cyd-01"
#define HEARTBEAT_SEC   30
```

Cada placa registra-se no servidor com `device_id` próprio. Mesma rede, mesmo `SERVER_URL`, papéis diferentes.

### Boot

```
1. S3: WiFi.begin() → POST /api/v1/health { "role": "s3" }
2. CYD: WiFi.begin() → POST /api/v1/health { "role": "cyd" }
3. UART: PAIR:HELLO → PAIR:OK → PAIR:READY (ambos)
4. CYD → S3: CONFIG:wifi_* + STREAM:ON
5. S3: loop captura → POST /camera/frame → FRAME:id,ok
6. CYD: poll /camera/frame/info + /display → TFT
```

Se a rede cair, ambos reconectam (backoff 1 s → 2 s → 5 s → 30 s). UART mantém o par operacional para alertas locais; SD acumula fila até a rede voltar.

---

## UART — bus interno do par

Pinos e fiação: [HARDWARE.md](HARDWARE.md). Formato das mensagens: [PROTOCOL.md](PROTOCOL.md).

UART e Wi-Fi **não se substituem** — complementam.

| Situação | UART | Wi-Fi |
|----------|:----:|:-----:|
| Pareamento e CONFIG | ✅ | — |
| Ligar/pausar stream (`STREAM:ON/OFF`) | ✅ | — |
| Aviso de frame pronto (`FRAME:id,ok`) | ✅ | — |
| JPEG bruto S3 → server | ❌ | ✅ |
| Preview TFT (display processado) | ❌ | ✅ |
| Health / config / OTA | — | ✅ |
| Botão BOOT (`CAPTURE` manual) | — | ✅ |

### Mensagens (texto, 460800 baud)

Detalhes: [PROTOCOL.md](PROTOCOL.md).

**S3 → CYD**

```
PAIR:HELLO,ID=s3-cam-01
PAIR:READY
FRAME:42,ok
FRAME:43,err
HEARTBEAT
```

**CYD → S3**

```
PAIR:OK,ID=cyd-01
PAIR:READY
CONFIG:wifi_ssid=...
CONFIG:wifi_pass=...
CONFIG:server_url=http://IP:18124
STREAM:ON
STREAM:OFF
CAPTURE
ACK:HEARTBEAT
```

**Sem payload binário na UART.**

---

## API do PC (servidor)

Base: `http://<ip-do-pc>:18124`

| Método | Endpoint | Quem chama | Descrição |
|--------|----------|------------|-----------|
| `POST` | `/api/v1/health` | S3 + CYD | Heartbeat e status do par |
| `POST` | `/api/v1/camera/frame` | S3 | JPEG bruto + `frame_id` |
| `GET` | `/api/v1/camera/frame/info` | CYD | Poll leve (`since`, `changed`) |
| `GET` | `/api/v1/camera/frame/display` | CYD | JPEG 320×201 processado |
| `GET` | `/api/v1/config` | S3 + CYD | Config por dispositivo |
| `GET` | `/api/v1/ui/state` | CYD | Estado da tela (overlay) |
| `GET` | `/api/v1/firmware/*` | S3 + CYD | OTA |
| `POST` | `/api/v1/events` | — | ⬜ planejado |
| `POST` | `/api/v1/media` | — | ⬜ planejado |
| `POST` | `/api/v1/chat` | — | ⬜ planejado |
| `WS` | `/api/v1/pair/stream` | — | ⬜ planejado |

### Exemplo — upload câmera (S3 → PC, v1)

```json
POST /api/v1/camera/frame?device_id=s3-cam-01&frame_id=42
Content-Type: image/jpeg

<JPEG bruto QVGA ~9 KB>
```

Resposta: `{ "ok": true, "frame_id": 42, "raw_size": 9400, "display_size": 7200 }`

### Exemplos planejados (não implementados)

**Evento (S3 → PC):**
{
  "device_id": "s3-cam-01",
  "event_id": 42,
  "ts": "2026-06-10T14:32:00",
  "type": "motion",
  "confidence": 0.87
}
```

### Exemplo — estado UI (PC → CYD, parcialmente implementado)

```json
{
  "screen": "timeline",
  "title": "Eventos de hoje",
  "lines": [
    "14:32 — pessoa desconhecida",
    "16:10 — João (conhecido)"
  ],
  "alert": null
}
```

### Exemplo — chat (CYD → PC, planejado)

```json
{
  "device_id": "cyd-01",
  "question": "Quem passou ontem à tarde?"
}
```

O LLM (Ollama no PC) e eventos são **planejados** — v1 foca stream de câmera via HTTP.

---

## Fluxos de trabalho

### 1. Stream contínuo (v1)

```
[CYD] PAIR:READY → STREAM:ON
[S3]  captura QVGA bruto → POST /camera/frame
[S3]  UART: FRAME:N,ok
[CYD] GET /info → GET /display → blit TFT
```

### 2. Captura manual

```
[CYD] botão BOOT
  → UART: CAPTURE → [S3] POST /camera/frame
  → UART: FRAME:N,ok → [CYD] fetch imediato /display
```

### 3. Upload falhou

```
[S3] POST falha → UART: FRAME:N,err
[CYD] mantém último frame; poll continua
```

### 4. PC offline — autonomia parcial

```
[S3] detecta Wi-Fi off → upload skip; UART FRAME:err
[CYD] alerta na UI; fila SD / proxy — planejado
```

---

## Pipeline em camadas

```
Etapa 1 (captura)          Etapa 2 (I/O + UI)         Etapa 3 (processamento)
──────────────────         ──────────────────         ─────────────────────
     S3-CAM            →        CYD               →         PC
  JPEG bruto + UART          poll HTTP + TFT            crop · sharpen · JPEG display
```

---

## Layout previsto no monorepo

```
edge-pair/
├── README.md                 ← este documento
├── PROTOCOL.md               ← UART texto-only (460800)
└── HARDWARE.md

goouuu-esp32-s3-cam/uart-peer/   ← edge-s3
esp32-cyd/uart-peer/             ← edge-cyd
pc-server/                       ← FastAPI + Pillow + OTA
```

---

## Relação com mini-games

O projeto [mini-games](../goouuu-esp32-s3-cam/mini-games/) usa a mesma GOOUUU com OLED externo e **não** utiliza câmera, SD ou Wi-Fi. É um firmware independente — prova de toolchain e pinout.

O **edge pair** é a evolução que liga S3-CAM + CYD + PC para visão, UI rica e IA na LAN.

---

## Status

| Componente | Status |
|------------|--------|
| Arquitetura (este doc) | ✅ v1 stream HTTP |
| [HARDWARE.md](HARDWARE.md) (fiação UART) | ✅ 460800 |
| [PROTOCOL.md](PROTOCOL.md) (UART texto) | ✅ alinhado ao firmware |
| S3 `uart-peer` | ✅ QVGA bruto + upload + UART |
| CYD `uart-peer` | ✅ poll `/display` + TFT |
| `pc-server` | ✅ health / config / camera / OTA |
| Eventos / LLM / chat | ⬜ planejado |

---

## Licença

[MIT](../LICENSE) — Copyright (c) 2026 Luiz Eduardo
