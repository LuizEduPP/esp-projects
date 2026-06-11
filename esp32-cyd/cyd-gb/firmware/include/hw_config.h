#pragma once
#include <stdint.h>

// ─── Display (portrait 240×320 — USB na base da CYD) ───────────────────────
#define TFT_ROTATION   0
#define TFT_PIN_BL     21
#define SCREEN_W       240
#define SCREEN_H       320
#define SCREEN_CX      (SCREEN_W / 2)

#define TOUCH_PIN_CS    33
#define TOUCH_PIN_IRQ   36
#define TOUCH_PIN_MOSI  32
#define TOUCH_PIN_MISO  39
#define TOUCH_PIN_CLK   25

#define SD_PIN_CS 5
#define SD_PIN_MOSI 23
#define SD_PIN_MISO 19
#define SD_PIN_SCK 18

#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17

// ─── GameBoy ────────────────────────────────────────────────────────────────
#define GB_SCREEN_W 160
#define GB_SCREEN_H 144

#define GAME_H 200
#define CTRL_Y 200
#define CTRL_H (SCREEN_H - CTRL_Y)

// ─── Barra de controles (2 faixas) ──────────────────────────────────────────
// Faixa 1: utilitários (MENU / SELECT / START)
#define CTRL_ROW1_Y  (CTRL_Y + 16)
#define CTRL_DIV_Y   (CTRL_Y + 38)

// Faixa 2: D-pad + A/B
#define BTN_MENU_X   36
#define BTN_MENU_Y   CTRL_ROW1_Y
#define BTN_MENU_W   56
#define BTN_MENU_H   24

#define BTN_SE_X    108
#define BTN_SE_Y    CTRL_ROW1_Y
#define BTN_SE_W     50
#define BTN_SE_H     24

#define BTN_ST_X    178
#define BTN_ST_Y    CTRL_ROW1_Y
#define BTN_ST_W     50
#define BTN_ST_H     24

// Faixa 2: D-pad + A/B
#define DPAD_CX       52
#define DPAD_CY      (CTRL_Y + 78)
#define DPAD_ARM       36
#define DPAD_THICK     15
#define DPAD_HIT_EXTRA 10

#define BTN_A_X       212
#define BTN_A_Y      (CTRL_Y + 64)
#define BTN_A_R        19

#define BTN_B_X       176
#define BTN_B_Y      (CTRL_Y + 94)
#define BTN_B_R        19
#define BTN_TOUCH_PAD   3

// Limites das zonas multitouch (touch_input.cpp)
#define ZONE_DPAD_X_MAX   106
#define ZONE_ACTION_X_MIN 118
