# Hardware — Mini Games

Bill of materials and wiring for **this firmware only**. The on-board camera, microSD slot, Wi-Fi, and audio are not used.

## Required parts

| Qty | Part | Notes |
|-----|------|-------|
| 1 | GOOUUU ESP32-S3-CAM | ESP32-S3 + OV2640 (camera unused) |
| 1 | SSD1306 OLED 128×64 | I2C, 0.96″, **3.3 V only** |
| 5 | Tactile push buttons | Active-low with internal pull-ups |
| 1 | Breadboard (optional) | 400- or 830-point |
| — | Jumper wires | M–F for ESP header · M–M on breadboard |
| 1 | USB-C cable | Data + power for flash/monitor, **or** data-only if ESP is powered elsewhere |

No external resistors, amplifier, microphone, speaker, LED, or SD card are required.

## Power

**Recommended (development):** power the ESP from **USB-C**. Feed the OLED from the ESP **3V3** pin and a common **GND**.

**Bench supply:** if the ESP is powered from **5V + GND** on the header, do **not** connect USB VBUS at the same time. Use a **data-only** USB-C cable for serial and flashing.

Enable **USB CDC On Boot** (already set in `platformio.ini`).

## Pin map

| Function | ESP silk | GPIO |
|----------|----------|------|
| OLED SDA | TX0 | 43 |
| OLED SCL | RX0 | 44 |
| Button Down | G2 | 2 |
| Button Up | G21 | 21 |
| Button Left | G14 | 14 |
| Button Right | G47 | 47 |
| Button A | G1 | 1 |

### GPIO to avoid

Do not use these for OLED or buttons:

| GPIO | Reason |
|------|--------|
| 4–18, 8–13, 15, 16 | Camera (module internal) |
| 35–42, 45 | PSRAM / microSD |
| 48 | On-board WS2812 RGB LED |
| 0, 3, 46 | Strapping pins |

Native USB: D+ **20**, D− **19** (no jumpers).

## OLED (I2C)

Typical module silk: **GND · VDD · SCK · SDA**

| OLED pin | Connect to |
|----------|------------|
| GND | GND |
| VDD | **3.3 V** (ESP 3V3 or breadboard 3.3 V rail) |
| SCK | GPIO **44** |
| SDA | GPIO **43** |

I2C address is usually **0x3C** (sometimes 0x3D). Firmware probes both.

## Buttons

Firmware configures **INPUT_PULLUP**. Each button:

```
GPIO pin ── button ── GND
```

Released = HIGH, pressed = LOW.

Suggested layout (left → right): **Down · Up · Left · Right · A**

## Wiring sketch

```
                    ESP32-S3-CAM (GOOUUU)
                    ┌─────────────────┐
         3V3 ───────┤ 3V3             │
         GND ───────┤ GND             │
    GPIO 43 ───────┤ TX0  (OLED SDA) │
    GPIO 44 ───────┤ RX0  (OLED SCL) │
    GPIO  2 ───────┤ G2   (Down)    ├──┐
    GPIO 21 ───────┤ G21  (Up)      ├──┤ each button
    GPIO 14 ───────┤ G14  (Left)    ├──┤ to GND
    GPIO 47 ───────┤ G47  (Right)   ├──┤
    GPIO  1 ───────┤ G1   (A)       ├──┘
                    │ USB-C (CDC)   │
                    └─────────────────┘

    OLED: VDD→3V3  GND→GND  SDA→43  SCL→44
```

## Assembly checklist

- [ ] OLED on **3.3 V**, not 5 V
- [ ] SDA → GPIO 43, SCL → GPIO 44
- [ ] Five buttons from GPIO to GND (no extra resistors)
- [ ] Common ground between OLED, buttons, and ESP
- [ ] No TTL USB adapter on GPIO 43/44 while OLED is wired there
- [ ] USB: either powers the board **or** is data-only when using header 5 V

## Flash / serial

```bash
yarn fw:flash
yarn fw:monitor
```

Default port in `platformio.ini`: `/dev/ttyACM0` (native USB CDC).
