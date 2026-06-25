#pragma once

// GOOUUU ESP32-S3-CAM — see HARDWARE.md (ESP32-S3 map + Folio wiring).
//
// INMP441 I2S (module pin "SD" = serial data, NOT the onboard microSD slot):
//   WS=GPIO1  SCK=GPIO2  DOUT=GPIO21
// Keyestudio KS5028 diagram uses DOUT=GPIO42 (MTMS / JTAG block) — do not use on Folio.
//
// Strap pins: GPIO0, GPIO3, GPIO45, GPIO46 — do not use for peripherals.

#ifndef PIN_I2S_WS
#define PIN_I2S_WS  1
#endif
#ifndef PIN_I2S_SCK
#define PIN_I2S_SCK 2
#endif
#ifndef PIN_I2S_DOUT
#define PIN_I2S_DOUT 21
#endif
/** INMP441 datasheet label for the data pin — not the microSD card. */
#define PIN_I2S_SD PIN_I2S_DOUT

#define CAM_LEDC_CHANNEL LEDC_CHANNEL_1
#define CAM_LEDC_TIMER   LEDC_TIMER_1

#define CAM_PIN_PWDN   -1
#define CAM_PIN_RESET  -1
#define CAM_PIN_XCLK   15
#define CAM_PIN_SIOD   4
#define CAM_PIN_SIOC   5
#define CAM_PIN_D7     16
#define CAM_PIN_D6     17
#define CAM_PIN_D5     18
#define CAM_PIN_D4     12
#define CAM_PIN_D3     10
#define CAM_PIN_D2     8
#define CAM_PIN_D1     9
#define CAM_PIN_D0     11
#define CAM_PIN_VSYNC  6
#define CAM_PIN_HREF   7
#define CAM_PIN_PCLK   13
