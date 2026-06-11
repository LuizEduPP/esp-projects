#pragma once

// Sunton CYD 2432S028 — UART híbrido P1 + P3
// Driver TFT: ILI9341 (R/Rv2) ou ST7789 (Rv3) — ver platformio.ini board
// P1 silk: VIN · TX · RX · GND  —  P3 silk: GND · GPIO35 · GPIO22 · GPIO21
//
// TX = GPIO 1  (P1 TX → S3 RX0 / GPIO 44)
// RX = GPIO 35 (P3 GPIO35 ← S3 TX0 / GPIO 43)
//
// Não use P1 RX (GPIO 3): compartilhado com CH340 — RX falha mesmo sem USB.

#define PIN_PEER_TX  1
#define PIN_PEER_RX  35
#define PEER_UART_BAUD 460800
