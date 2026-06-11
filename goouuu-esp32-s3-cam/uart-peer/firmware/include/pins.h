#pragma once

// GOOUUU ESP32-S3-CAM → CYD (híbrido P1+P3 na CYD) — silk TX0 / RX0 on header
// S3 TX0 (43) → CYD P3 GPIO35 | S3 RX0 (44) ← CYD P1 TX (GPIO1)
// Avoid GPIO 4-18 (camera), 35-42/45 (SD/PSRAM), 0/3/46 (strapping)

#define PIN_PEER_TX  43  // silk TX0
#define PIN_PEER_RX  44  // silk RX0
#define PEER_UART_BAUD 460800
