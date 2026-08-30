# dash-api

Aggregates everything the ESP32-C3 panel shows into a single JSON document, so the board makes
one request instead of a dozen TLS handshakes it barely has RAM for.

```bash
yarn dash-api:dev     # local, port 8090
yarn dash-api:up      # docker compose
yarn dash-api:logs
```

`GET /dash` returns weather, air quality, moon, news, market, rates, holiday, "on this day",
space, GitHub and the AI sentence — around 1.6 KB. `GET /health` reports how many seconds ago
each source last succeeded. `POST /refresh` forces a full refresh.

The server also owns the **look** of the panel and its **firmware**, so the board is only a
renderer. Open `/` to pick one of the 30 themes; the board applies it within 10 seconds without
being reflashed.

## The UI document

Data and appearance travel on separate channels, so neither invalidates the other: `/dash` every
5 minutes, the UI only when its version changes.

| Route | Purpose |
|-------|---------|
| `GET /` | editor: 30 themes previewed 128x128, click to apply, click a screen to hide it |
| `GET /ui/version` | `{version, theme, pages[]}` — the ~300 byte target of the board's polling |
| `GET /ui/page/:id` | one screen, ~1 KB, fetched and cached individually in LittleFS |
| `GET /ui` | the whole document, used by the editor |
| `POST /ui` | `{theme?, hidden?}`, bumps `version` and persists to `data/ui.json` |

A screen is a background plus absolutely positioned items. `bind` is a path into the `/dash`
payload, and `fmt` is a printf spec applied on the board:

```json
{ "t": "label", "x": 50, "y": 26, "w": 72, "font": 28, "color": "#ffffff",
  "bind": "weather.temp", "fmt": "%.0f" }
```

Widgets: `label` (with `wrap` and `scroll`), `plate`, `rule`, `arc`, `bar`, `chart`, `icon`,
`moon`, `needle`. Backgrounds: `solid`, `gradient`, `grid`, `dots`, each with optional blurred
`blobs`.

Two constraints worth knowing. Fonts are compiled into the firmware at fixed sizes — 10, 12, 14,
16, 20 and 28 — so a document asking for anything else falls back to 12; that is why the serif
and seven-segment mocks are not in the catalog. And `bind` also reaches fields the board
generates itself (`time`, `date`, `net.*`, `sys.*`, `timer.*`, `sun.*`), which live in the same
namespace as the server payload.

## OTA

`GET /firmware/version` returns version, sha256 and size; `GET /firmware/bin` streams the image.
`POST /firmware?version=...` publishes it, guarded by `DASH_OTA_TOKEN` when set. From the
monorepo, `yarn dash:publish` sends the binary that `yarn dash:build` just produced.

The board compares against the `DASH_FW_VERSION` compiled into it, writes to the idle OTA
partition, verifies the hash before marking it valid, and only then reboots. A download that
dies halfway leaves the running partition untouched.

## Why it exists

The firmware used to call ten APIs directly. That meant HTTPS on a chip with ~400 KB of RAM,
parsing a 100 KB RSS feed off the socket, folding UTF-8 accents by hand and re-deploying the
board whenever an endpoint changed. All of that now happens here, and the payload arrives as
plain ASCII the LVGL Montserrat subset can actually render.

Every source is cached and refreshed on a timer, so a request from the board never waits on an
upstream API and a flaky source degrades to stale data instead of a blank screen.

## Sources

All free, none requires a key:

| Field | Source |
|-------|--------|
| weather, air, rain, wind, sun, UV | [Open-Meteo](https://open-meteo.com/) |
| moon | computed locally from the synodic period |
| news | [Agência Brasil](https://agenciabrasil.ebc.com.br/) RSS |
| market | [AwesomeAPI](https://docs.awesomeapi.com.br/) |
| rates, holidays | [BrasilAPI](https://brasilapi.com.br/) |
| history | Portuguese Wikipedia REST feed |
| space | [Open Notify](http://open-notify.org/) |
| dev | GitHub |
| ai | Ollama |

Two feeds that look obvious but do not work: `g1.globo.com/rss` answers HTTP 426 because it
requires HTTP/2, and Folha's RSS is ISO-8859-1, which mangles the accent folding.

`DASH_GITHUB_TOKEN` is optional. Without it the GitHub numbers come from the public events feed,
which hides private work and only spans 90 days. With a classic token the contributions graph is
used instead, so private commits are counted.

## Deploying behind Traefik

The healthcheck must target `127.0.0.1`, not `localhost`. Inside Alpine `localhost` resolves to
`::1` first while the server listens on IPv4 only, so the check fails, the container goes
unhealthy, and Traefik silently drops it — the domain then answers 404 with no error logged
anywhere.
