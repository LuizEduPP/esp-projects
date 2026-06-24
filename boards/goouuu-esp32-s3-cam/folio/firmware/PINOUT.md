# ESP32-S3 pinout — Folio / GOOUUU ESP32-S3-CAM

References:

- ESP32-S3 devkit silk labels (connector tables below)
- [Which pins can I use on an ESP32-S3](https://www.fourbeescircuits.com/que-pines-puedo-usar-esp32-s3) — safe / caution / avoid summary

GOOUUU reuses the same GPIO numbers. Onboard **camera**, **microSD**, and **PSRAM** claim extra pins beyond a bare devkit.

---

## Strapping pins (never use for peripherals)

| GPIO | Role |
|------|------|
| **0** | Boot mode (pulled up = SPI flash boot) |
| **3** | JTAG strapping |
| **45** | VDD_SPI voltage select (pulled down) |
| **46** | Boot mode / ROM messages (pulled down) |

## Other pins to avoid in projects

| GPIO | Reason |
|------|--------|
| **26–32** | SPI flash (internal) |
| **19–20** | USB D− / D+ when using native USB |
| **33–37** | PSRAM on many S3 modules (🟢 board-dependent) |
| **39–42** | JTAG when using in-circuit debug (🟢 OK if JTAG unused) |

## Safe vs caution — Folio-relevant GPIOs

| GPIO | Safety | Functions | Folio use |
|------|--------|-----------|-----------|
| **1** | ✔️ Safe | ADC1_CH0, TOUCH1 | INMP441 **WS** |
| **2** | ✔️ Safe | ADC1_CH1, TOUCH2 | INMP441 **SCK** |
| **21** | ✔️ Safe | GPIO | INMP441 **DOUT** (recommended) |
| **14** | ✔️ Safe | ADC2_CH3, FSPIWP | Alt DOUT probe |
| **47** | ✔️ Safe* | SPICLK_P | Alt DOUT probe |
| **4–18** | ✔️ Safe* | ADC, camera bus | Onboard **OV2640** (`pins.h`) |
| **38** | ✔️ Safe | FSPIWP | GOOUUU **microSD CMD** |
| **39** | 🟢 JTAG | MTCK | GOOUUU **microSD CLK** |
| **40** | 🟢 JTAG | MTDO | GOOUUU **microSD D0** |
| **41** | 🟢 JTAG | MTDI | — |
| **42** | 🟢 JTAG | MTMS | Keyestudio kit DOUT — **do not use for I2S** |
| **35–37** | 🟢 PSRAM | SPIIO6–7, SPIDQS | Onboard PSRAM on GOOUUU |
| **43–44** | ⚠️ UART0 | U0TXD / U0RXD | Serial (TX/RX) |
| **0, 3, 45, 46** | ❌ Strap | — | Do not use |

\*Safe on chip; Folio already assigns camera pins to the onboard sensor.

### INMP441 vs microSD vs Keyestudio kit

| Name on module / docs | What it is | Folio GPIO |
|-----------------------|------------|------------|
| INMP441 **SD** | I2S serial **data** (DOUT) | **21** (not 42) |
| microSD slot | FAT32 storage (SD_MMC) | CLK **39**, CMD **38**, D0 **40** |
| Keyestudio diagram **SD → 42** | Wrong for Folio — GPIO42 = **MTMS/JTAG** | Rewire to **21** |

---

## connector — left side

| Label | GPIO | Notes |
|-------|------|-------|
| TX | 43 | U0TXD |
| RX | 44 | U0RXD |
| 1 | 1 | Folio INMP441 WS |
| 2 | 2 | Folio INMP441 SCK |
| 42 | 42 | MTMS / JTAG |
| 41 | 41 | MTDI |
| 40 | 40 | MTDO — microSD D0 |
| 39 | 39 | MTCK — microSD CLK |
| 38 | 38 | RGB LED (Wokwi) — microSD CMD |
| 37 | 37 | SPIDQS / PSRAM |
| 36 | 36 | SPIIO7 / PSRAM |
| 35 | 35 | SPIIO6 / PSRAM |
| 0 | 0 | BOOT strap |
| 45 | 45 | Strap |
| 48 | 48 | SPICLK_N |
| 47 | 47 | SPICLK_P |
| 21 | 21 | **Folio INMP441 DOUT** |
| 20 | 20 | USB_D+ / ADC2_9 |
| 19 | 19 | USB_D− / ADC2_8 |

## connector — right side

| Label | GPIO | Notes |
|-------|------|-------|
| 3V3 | 3.3V | Power |
| EN | — | Reset |
| 4–7 | 4–7 | Camera SCCB / sync |
| 15–18 | 15–18 | Camera |
| 8–14 | 8–14 | Camera data / PCLK |
| 3 | 3 | Strap / ADC1_2 |
| 46 | 46 | Strap |
| 5V | 5V | Power in |

## Folio firmware map

Defined in `include/pins.h`:

| Function | GPIO |
|----------|------|
| INMP441 WS / SCK / DOUT | 1 / 2 / **21** |
| Camera | 4–18 — see `CAM_PIN_*` in `pins.h` |

Build overrides: `platformio.ini`

- `-DPIN_I2S_DOUT=21`
