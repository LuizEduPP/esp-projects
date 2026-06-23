#pragma once

// Pinagem GOOUUU ESP32-S3-CAM — única fonte de verdade
// Esq L9110S: 1,2,14,47 | Dir L9110S: 21,48,19,20 | Cam: 4–18 | LEDC ch1 = XCLK

#define PIN_L_IA1   1
#define PIN_L_IB1   2
#define PIN_L_IA2   14
#define PIN_L_IB2   47
#define PIN_R_IA1   21
#define PIN_R_IB1   48   // LED placa; driver usa GPIO digital
#define PIN_R_IA2   19
#define PIN_R_IB2   20

#define CAM_LEDC_CHANNEL  LEDC_CHANNEL_1
#define CAM_LEDC_TIMER    LEDC_TIMER_1
// Motores = GPIO digital (sem LEDC/PWM). Camera usa LEDC ch1 no GPIO15.

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
