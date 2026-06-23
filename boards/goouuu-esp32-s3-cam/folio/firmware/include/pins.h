#pragma once

// GOOUUU ESP32-S3-CAM — folio-node (passive witness)
// Camera pins match rc-car; I2S INMP441 from project-twelve reference.

#define PIN_I2S_WS  1
#define PIN_I2S_SCK 2
#define PIN_I2S_SD  42

// Optional PIR / motion (-1 = disabled)
#define PIN_MOTION  3

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
