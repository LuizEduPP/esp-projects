# RC Car — seguidor IA

Pinagem: `firmware/include/pins.h`

```bash
yarn rc-car:upload   # flash ESP32
yarn rc-car:ai       # LM Studio + painel http://localhost:8765
```

ESP32 expõe `GET /capture`, `POST /control` (`{"left":n,"right":n}`), `GET /status`.

Detecção: modelo vision → `person`, `confidence`, `cx`, `cy` → script calcula motores com histerese.

Teste motor: `curl -X POST http://IP/control -H 'Content-Type: application/json' -d '{"left":180,"right":180}'`

Variáveis: `ESP_URL`, `LM_MODEL`, `MIN_CONFIDENCE`, `VIEW_PORT` — ver `scripts/ai-follow.mjs`.
