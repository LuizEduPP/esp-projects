#pragma once

// L9110S #1 — esquerda
#define PIN_L_IA1  1
#define PIN_L_IB1  2
#define PIN_L_IA2  14
#define PIN_L_IB2  47

// L9110S #2 — direita (fios fisicos: 21, 48, 19, 20)
// GPIO48 = LED RGB na placa — firmware usa GPIO digital, nao NeoPixel.
#define PIN_R_IA1  21
#define PIN_R_IB1  48
#define PIN_R_IA2  19
#define PIN_R_IB2  20

#define PIN_RGB_LED  48
