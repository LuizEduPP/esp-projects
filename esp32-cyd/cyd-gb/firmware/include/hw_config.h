#pragma once
#include <stdint.h>


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


#define GB_SCREEN_W 160
#define GB_SCREEN_H 144

/*
 * Layout 240×320 — ver mock/ui-wireframe.svg
 *   status  0..24
 *   jogo   24..242
 *   deck  242..320 (78px)
 */
#define STATUS_H       24
#define CTRL_H         78
#define CTRL_Y         (SCREEN_H - CTRL_H)
#define CTRL_CY        (CTRL_Y + CTRL_H / 2)
#define BEZEL_Y        STATUS_H
#define BEZEL_H        (CTRL_Y - BEZEL_Y)
#define GAME_W         238
#define GAME_H         214
#define GAME_X         1
#define GAME_Y         (BEZEL_Y + 2)

#define CTRL_Y_MIN     CTRL_Y
#define ZONE_STICK_Y_MIN (CTRL_Y + 6)

#define BTN_PAUSE_W    36
#define BTN_PAUSE_H    STATUS_H
#define BTN_PAUSE_L    (SCREEN_W - BTN_PAUSE_W)
#define BTN_PAUSE_T    0
#define BTN_PAUSE_CX   (BTN_PAUSE_L + BTN_PAUSE_W / 2)
#define BTN_PAUSE_CY   (STATUS_H / 2)
#define BTN_TOUCH_PAD  4

/* SELECT / START — small circles, stacked center deck */
#define BTN_UTIL_X     SCREEN_CX
#define BTN_SE_X       BTN_UTIL_X
#define BTN_ST_X       BTN_UTIL_X
#define BTN_UTIL_R     12
#define BTN_UTIL_HIT   (BTN_UTIL_R + BTN_TOUCH_PAD)
#define BTN_UTIL_GAP   4
#define BTN_SE_Y       (CTRL_CY - BTN_UTIL_R - BTN_UTIL_GAP / 2)
#define BTN_ST_Y       (CTRL_CY + BTN_UTIL_R + BTN_UTIL_GAP / 2)

/* A / B — side by side right, full deck height, flush edges */
#define BTN_AB_GAP     2
#define BTN_B_L        155
#define BTN_B_W        40
#define BTN_A_W        43
#define BTN_A_L        (SCREEN_W - BTN_A_W)
#define BTN_AB_T       CTRL_Y
#define BTN_AB_H       CTRL_H
#define BTN_B_CX       (BTN_B_L + BTN_B_W / 2)
#define BTN_A_CX       (BTN_A_L + BTN_A_W / 2)
#define BTN_AB_CY      CTRL_CY

#define ZONE_STICK_X_MAX   110
#define ZONE_UTIL_X_MIN    88
#define ZONE_UTIL_X_MAX    152
#define ZONE_ACTION_X_MIN  BTN_B_L

/* Analógico — altura máxima do deck */
#define STICK_CX        52
#define STICK_CY        CTRL_CY
#define STICK_BASE_R    34
#define STICK_KNOB_R    10
#define STICK_HIT_EXTRA 8
#define STICK_RANGE     (STICK_BASE_R - STICK_KNOB_R - 2)
