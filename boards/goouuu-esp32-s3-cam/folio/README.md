# Folio — testemunha passiva do dia

O ESP32 **não responde**. Ele ouve e vê; o PC processa, memoriza e escreve a **crônica interpretada** do seu dia.

## Stack

| Camada | O quê |
|--------|--------|
| **folio-node** (ESP32-S3-CAM + INMP441) | Push áudio 1 s + frame JPEG / 60 s (+ motion) |
| **folio-brain** (PC, Node 22+) | Ingest, Whisper STT, vision LM Studio, `node:sqlite` |
| **Digest A→D** | Fatos → interpretação → crítica → prosa |

## Hardware

- GOOUUU ESP32-S3-CAM (câmera onboard)
- INMP441 I2S — GPIO 1 (WS), 2 (SCK), 42 (SD)
- Motion (opcional) — GPIO 3

Sem speaker, sem OLED.

## Configurar firmware

Edite `firmware/platformio.ini`:

- `WIFI_SSID` / `WIFI_PASS`
- `FOLIO_BRAIN_URL` — IP do PC, ex. `http://192.168.1.100:8770`
- `FOLIO_DEVICE_ID` — id estável do nó

## Quick start

```bash
# Na raiz do monorepo
yarn install
yarn folio:brain          # terminal 1 — ingest + UI http://localhost:8770

yarn folio:flash          # terminal 2 — grava ESP
yarn folio:monitor        # logs do nó

# Após acumular dados no dia
yarn folio:digest         # pipeline A→D + salva ~/.folio/digests/YYYY-MM-DD.md
```

## Variáveis de ambiente (brain)

| Var | Default | Descrição |
|-----|---------|-----------|
| `FOLIO_PORT` | `8770` | Porta HTTP do brain |
| `FOLIO_DATA_DIR` | `~/.folio` | SQLite + áudio + frames |
| `LM_URL` | `http://127.0.0.1:1234/v1/chat/completions` | LM Studio |
| `FOLIO_MODEL_FAST` | `mistralai/ministral-3-3b` | Ingest + passes A/C |
| `FOLIO_MODEL_DEEP` | same | Passes B/D (use modelo maior se tiver) |
| `FOLIO_WHISPER_BIN` | `whisper` | CLI OpenAI Whisper |
| `FOLIO_WHISPER_MODEL` | `base` | Modelo Whisper |
| `FOLIO_EPISODE_GAP_MIN` | `12` | Minutos de silêncio → novo episódio |

## Endpoints ESP (local)

| Método | Path | Função |
|--------|------|--------|
| GET | `/health` | Diagnóstico |
| GET | `/capture` | JPEG (pull manual) |
| POST | `/pause` | Pausa captura |
| POST | `/resume` | Retoma |

Push automático para o brain: `POST /ingest/audio`, `/ingest/frame`, `/ingest/event`.

## Pipeline digest (A→D)

Documentação completa: [docs/SCHEMA.md](docs/SCHEMA.md)

1. **Pass A** — ledger factual com IDs de evidência (`utt:`, `frm:`, `ep:`)
2. **Pass B** — arco, decisões reais, loops abertos, padrões
3. **Pass C** — crítica: remove claims sem evidência
4. **Pass D** — carta em prosa (pt-BR), sem templates markdown

Episódios: clusters de fala por gap configurável + extração semântica multimodal.

## Privacidade

- `POST /pause` no ESP ou desligar o brain
- Áudio bruto em `~/.folio/audio/` — configure retenção manualmente
- LED da placa = atividade WiFi (não há LED “gravando” dedicado no MVP)

## Speaker enrollment (opcional)

```bash
yarn folio:enroll luiz "Luiz Eduardo"
```

Metadados apenas; embedding de voz é extensão futura.
