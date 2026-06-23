# RC Car — AI follower

Pin map: `firmware/include/pins.h`

```bash
yarn rc-car:upload
yarn rc-car:monitor    # serial logs [motor] [http] [test]
yarn rc-car:diag       # HTTP battery test + motor-diag.log
yarn rc-car:ai
```

### Endpoints + logs

| Endpoint | Serial log | Description |
|----------|------------|-------------|
| `GET /diag` | `[http] GET /diag` | Full state + each GPIO level |
| `POST /control` | `[motor] SET L= R=` + GPIO readback | Motor command |
| `POST /test` | `[test] T0..T8` | On-ESP battery test (~12 s) |
| `GET /capture` | `[http] capture OK` | JPEG photo |

Troubleshooting: if `GPIO level=1` in diag but the motor does not spin → **wiring/power**. If `level=0` with L/R≠0 → software bug.
