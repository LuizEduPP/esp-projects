# RC Car — seguidor IA

Pinagem: `firmware/include/pins.h`

```bash
yarn rc-car:upload
yarn rc-car:monitor    # logs serial [motor] [http] [test]
yarn rc-car:diag       # bateria HTTP + motor-diag.log
yarn rc-car:ai
```

### Endpoints + logs

| Endpoint | Log serial | Descrição |
|----------|------------|-----------|
| `GET /diag` | `[http] GET /diag` | Estado completo + nível de cada GPIO |
| `POST /control` | `[motor] SET L= R=` + leitura GPIO | Comando motor |
| `POST /test` | `[test] T0..T8` | Bateria no ESP (~12s) |
| `GET /capture` | `[http] capture OK` | Foto JPEG |

Interpretação: se `GPIO level=1` no diag mas motor não gira → **fiação/alimentação**. Se `level=0` com L/R≠0 → bug software.
