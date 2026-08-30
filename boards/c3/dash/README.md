# dash

Desk panel for the Spotpear ESP32-C3 1.44″. The board no longer decides what it looks like: the
[dash-api](../../../services/dash-api) describes every screen as a JSON document, and this
firmware is a generic renderer for it.

```bash
cp firmware/platformio.local.ini.example firmware/platformio.local.ini
$EDITOR firmware/platformio.local.ini    # API base URL (WiFi is optional, see below)
yarn dash:flash
```

## How it works

Three channels, all pointing at `DASH_API_URL`:

| What | Route | When |
|------|-------|------|
| Data | `GET /dash` | every 5 min |
| Appearance | `GET /ui/version`, then `GET /ui/page/:id` | polled every 10 s, downloaded only on change |
| Firmware | `GET /firmware/version`, `GET /firmware/bin` | checked at boot and hourly |

Changing the theme means clicking a thumbnail on the server's web editor. The board notices the
new version within 10 seconds and rebuilds itself. The USB cable is now only needed if the board
locks up before it reaches the network.

## Rendering

`render.cpp` walks the document and creates LVGL objects: `label`, `plate`, `rule`, `arc`, `bar`,
`chart`, `icon`, `moon`, `needle`, over a `solid`, `gradient`, `grid` or `dots` background with
blurred blobs. Items carry absolute coordinates — LVGL's flex layout inside style-less containers
produced overlapping labels here, and at 128×128 explicit pixels are both predictable and easy to
verify.

Items with a `bind` are registered in a small table so `renderRefresh()` can push new values
without rebuilding the tree; that is what runs once per second for the clock.

Only the **visible** screen exists as LVGL objects. The old build created all 19 up front in an
`lv_tileview`, which is what killed the board when the screen count grew — it would boot to a
blank panel or freeze on the first page change, with nothing on serial. The cost is that page
turns fade instead of sliding. In exchange, the number of screens is limited by flash, not RAM.

Fonts are Montserrat 10/12/14/16/20/28, ASCII only. Accent folding happens on the server, so
anything reaching a label is already safe to draw.

## Binds

A `bind` is a dotted path — `weather.days.0.max`, `news.1` — resolved against a single JSON
document held in RAM. It contains the `/dash` payload plus the fields the board generates itself,
in the same namespace:

| Path | Origin |
|------|--------|
| `time`, `weekday`, `date` | local clock, refreshed every second |
| `sun.progress`, `sun.daylight` | computed from sunrise/sunset |
| `net.ssid`, `net.ip`, `net.rssi` | WiFi |
| `sys.heap`, `sys.uptime` | runtime |
| `timer.clock`, `timer.mode`, `timer.percent` | pomodoro |
| everything else | the server |

## Cache and offline boot

Each screen is written to LittleFS as `/ui/<id>.json`, with `/ui/meta.json` holding the version
and page order. Boot without a network renders straight from that cache. A page that fails to
download aborts the whole update, so the cache is never left half-new.

Screens are fetched one at a time rather than as one big document: 19 screens of JSON would not
survive being parsed into RAM at once, and one screen is about 1 KB.

## OTA

`app0` and `app1` are 6.4 MB each in `default_16MB.csv` while the image is ~1.4 MB, so OTA was
purely a software problem. The board compares its compiled-in `DASH_FW_VERSION` against
`/firmware/version`, streams the binary into the idle partition while hashing it, and only marks
it valid if the sha256 matches. A download that dies halfway leaves the running partition intact.

Publishing, after a build:

```bash
yarn dash:build
yarn dash:publish     # honours DASH_API_URL and DASH_OTA_TOKEN
```

Bump `DASH_FW_VERSION` in `platformio.ini` before publishing, otherwise the board sees the same
version and skips the update.

## Controls

| Button | GPIO | Action |
|--------|------|--------|
| Upper | 10 | next screen — in the menu, moves up |
| Lower | 8 | previous screen — in the menu, moves down |
| BOOT | 9 | open the screen menu — inside it, jump to the marked screen |
| BOOT (hold 3 s) | 9 | forget WiFi and re-provision |

The panel is rotated 180°, so the button the vendor calls "top right" (GPIO 8) is physically the
lower one — hence GPIO 10 is the one that moves forward.

The menu lists the screens by the titles in the document, so hiding or reordering screens in the
web editor changes the menu without touching the firmware. Data refresh is on a timer only; there
is no manual refresh button any more, since one request now covers every screen.

After `DASH_SCREEN_TIMEOUT_MS` of silence the panel goes near-black. It is not a real dimmer — the
backlight has no GPIO in its path, per the vendor schematic — but since the panel emits the same
light either way, painting almost-black pixels is the closest thing to 1% brightness. Any button
wakes it.

There is no battery readout: the PL4054 charges the LiPo but neither its status pin nor a voltage
divider reaches a GPIO. It would require soldering a 2:1 divider from BAT+ to GPIO 1.

## WiFi provisioning

Credentials are **not** compiled in. On first boot the board starts SmartConfig (ESP-Touch): open
the EspTouch app, send SSID and password, and it stores them in NVS. Hold BOOT for 3 s to switch
networks. `WIFI_SSID`/`WIFI_PASS` still work as a compile-time fallback.

## Config

Credentials go in `firmware/platformio.local.ini` (git-ignored); the `[secrets]` section exists so
the local file *adds* flags instead of replacing the whole `build_flags` list. Everything else
defaults in [`include/dash_config.h`](firmware/include/dash_config.h):

| Flag | Default |
|------|---------|
| `DASH_API_URL` | `https://dash-esp32.kmali.online` (base, without a path) |
| `DASH_API_INTERVAL_MS` | 5 min |
| `DASH_UI_POLL_MS` | 10 s |
| `DASH_OTA_INTERVAL_MS` | 1 h |
| `DASH_SCREEN_TIMEOUT_MS` | 60 s |

LVGL is configured entirely through `build_flags` with `LV_CONF_SKIP`, so there is no `lv_conf.h`.
Keep values free of parentheses: PlatformIO passes flags through a shell, and
`-DLV_MEM_SIZE="(40U*1024U)"` fails to even start the compiler. Memory comes from the system heap
(`LV_USE_STDLIB_MALLOC=LV_STDLIB_CLIB`), not LVGL's fixed pool.

## Notes

Rendering goes through a full-screen `GFXcanvas16` (32 KB) pushed in one `drawRGBBitmap`, so
nothing flickers. The panel runs on hardware SPI; the five-pin Adafruit constructor would silently
fall back to bit-bang and cost ~20× per full redraw.

NTP is considered valid only when the year is past 2020 — anything older means the RTC is still
holding its power-on default and never got a real sync.

The old host-side LVGL simulator is gone. The server's editor renders the same document the board
receives, so the browser preview replaced it — and unlike the simulator, it cannot drift from what
ships.
