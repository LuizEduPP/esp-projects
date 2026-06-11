#pragma once

// GOOUUU ESP32-S3-CAM — see README.md
// Avoid GPIO 4-18 (camera), 35-42/45 (SD/PSRAM), 3/46 (strapping)

#define PIN_OLED_SDA  43  // ESP TX0 — OLED SDA
#define PIN_OLED_SCL  44  // ESP RX0 — OLED SCL

// 5 buttons left to right: DOWN | UP | LEFT | RIGHT | A
#define PIN_BTN_DOWN  2
#define PIN_BTN_UP    21
#define PIN_BTN_LEFT  14
#define PIN_BTN_RIGHT 47
#define PIN_BTN_A     1
