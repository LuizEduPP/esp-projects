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

#define LED_R_PIN 4
#define LED_G_PIN 16
#define LED_B_PIN 17

#define BUZZER_PIN       1
#define BUZZER_LEDC_CH   4
#define BUZZER_LEDC_BITS 10

/* Layout UI — tokens compartilhados */
#define UI_PAD           12
#define UI_CONTENT_W     (SCREEN_W - UI_PAD * 2)
#define UI_CARD_R        8
#define UI_MODAL_R       10
#define UI_BADGE_H       22
#define UI_BADGE_R       6
#define UI_BTN_H         44

#define STATUS_H         32
#define HUD_BADGE_Y      ((STATUS_H - UI_BADGE_H) / 2)
#define HUD_LIVES_X      6
#define HUD_LIVES_W      50
#define HUD_TIER_X       60
#define HUD_TIER_W       66
#define HUD_SCORE_X      130
#define HUD_SCORE_W      66
#define HUD_PAUSE_X      202
#define HUD_PAUSE_W      30

#define UI_LIST_STRIPE_W 4

#define UI_SCROLLBAR_W   8
#define UI_SCROLLBAR_X   (SCREEN_W - UI_SCROLLBAR_W - 4)

#define UI_HDR_H         48
#define UI_LIST_TOP      (UI_HDR_H + 6)
#define UI_LIST_ROW_H    48
#define UI_LIST_ROW_W    (UI_CONTENT_W - UI_SCROLLBAR_W - 4)
#define UI_LIST_GAP      8
#define UI_LIST_ROW_STEP (UI_LIST_ROW_H + UI_LIST_GAP)
#define UI_LIST_VIEW_H   (SCREEN_H - UI_LIST_TOP - UI_PAD)
#define UI_LIST_BOT      SCREEN_H

#define SPLASH_MS            800
#define SPLASH_BAR_Y         (SCREEN_H - UI_PAD - 8)

#define UI_HDR_GEAR_X        (SCREEN_W - 44)
#define UI_HDR_GEAR_ZONE_X   (SCREEN_W - 52)
#define UI_HDR_GEAR_ZONE_W   52

#define PLAY_X       0
#define PLAY_Y       STATUS_H
#define PLAY_W       SCREEN_W
#define PLAY_H       (SCREEN_H - STATUS_H)
#define PLAY_MARGIN  8

/* Tetris — células quadradas (evita peças “esticadas”) */
#define TETRIS_CELL        13
#define TETRIS_CELL_W      TETRIS_CELL
#define TETRIS_CELL_H      TETRIS_CELL
#define TETRIS_BW          10
#define TETRIS_BH          20
#define TETRIS_BOARD_W     (TETRIS_BW * TETRIS_CELL_W)
#define TETRIS_BOARD_H     (TETRIS_BH * TETRIS_CELL_H)
#define TETRIS_PROX_W      36
#define TETRIS_GROUP_W     (TETRIS_BOARD_W + 8 + TETRIS_PROX_W)
#define TETRIS_OFF_X       ((PLAY_W - TETRIS_GROUP_W) / 2)
#define TETRIS_OFF_Y       ((PLAY_H - TETRIS_BOARD_H) / 2)
#define TETRIS_PROX_X      (TETRIS_OFF_X + TETRIS_BOARD_W + 8)
