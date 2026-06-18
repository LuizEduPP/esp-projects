#pragma once

// ── GOOUUU ESP32-S3-CAM — pinagem confirmada ──────────────────────────────
//
// Motores (2× L9110S, 4WD):
//   Esquerda: IA1=1  IB1=2  IA2=14 IB2=47
//   Direita:  IA1=21 IB1=48 IA2=19 IB2=20
//
// GPIO48 = LED RGB da placa + IB1 motor direito (somente digital, sem PWM).
//
// Câmera OV2640: GPIO 4–18 (sem conflito com motores).
// LEDC canal 1 reservado ao XCLK da câmera — motores não usam canal 1.

// L9110S esquerda
#define PIN_L_IA1  1
#define PIN_L_IB1  2
#define PIN_L_IA2  14
#define PIN_L_IB2  47

// L9110S direita
#define PIN_R_IA1  21
#define PIN_R_IB1  48   // = PIN_RGB_LED
#define PIN_R_IA2  19
#define PIN_R_IB2  20
#define PIN_RGB_LED  48

// LEDC dos motores (índice lógico → canal hardware; 255 = GPIO digital)
#define MOTOR_LEDC_L_IA1  0
#define MOTOR_LEDC_L_IB1  2   // pula ch1 (câmera)
#define MOTOR_LEDC_L_IA2  3
#define MOTOR_LEDC_L_IB2  4
#define MOTOR_LEDC_R_IA1  5
#define MOTOR_LEDC_R_IB1  255
#define MOTOR_LEDC_R_IA2  6
#define MOTOR_LEDC_R_IB2  7

#define CAM_LEDC_CHANNEL  LEDC_CHANNEL_1
#define CAM_LEDC_TIMER    LEDC_TIMER_1

// OV2640
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
