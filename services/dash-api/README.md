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
