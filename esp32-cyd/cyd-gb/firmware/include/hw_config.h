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

#define STATUS_H       24
#define CTRL_Y         239
#define CTRL_H         81
#define CTRL_CY        (CTRL_Y + CTRL_H / 2)
#define BEZEL_Y        STATUS_H
#define BEZEL_H        (CTRL_Y - BEZEL_Y)
#define GAME_X         0
#define GAME_Y         25
#define GAME_W         240
#define GAME_H         214

#define CTRL_Y_MIN     CTRL_Y
#define ZONE_STICK_Y_MIN (CTRL_Y + 4)

#define BTN_PAUSE_W    52
#define BTN_PAUSE_H    STATUS_H
#define BTN_PAUSE_L    188
#define BTN_PAUSE_T    0
#define BTN_PAUSE_CX   (BTN_PAUSE_L + BTN_PAUSE_W / 2)
#define BTN_PAUSE_CY   (STATUS_H / 2)
#define BTN_TOUCH_PAD  4

#define BTN_UTIL_X     120
#define BTN_SE_X       BTN_UTIL_X
#define BTN_ST_X       BTN_UTIL_X
#define BTN_UTIL_R     12
#define BTN_UTIL_HIT   (BTN_UTIL_R + BTN_TOUCH_PAD)
#define BTN_SE_Y       259
#define BTN_ST_Y       297

#define BTN_B_L        148
#define BTN_B_W        44
#define BTN_A_L        192
#define BTN_A_W        48
#define BTN_AB_T       CTRL_Y
#define BTN_AB_H       CTRL_H
#define BTN_B_CX       (BTN_B_L + BTN_B_W / 2)
#define BTN_A_CX       (BTN_A_L + BTN_A_W / 2)
#define BTN_AB_CY      CTRL_CY

#define ZONE_STICK_X_MAX   92
#define ZONE_UTIL_X_MIN    96
#define ZONE_UTIL_X_MAX    144
#define ZONE_ACTION_X_MIN  BTN_B_L

#define STICK_CX        48
#define STICK_CY        279
#define STICK_BASE_R    30
#define STICK_KNOB_R    10
#define STICK_HIT_EXTRA 8
#define STICK_RANGE     (STICK_BASE_R - STICK_KNOB_R - 2)

#define UI_STATUS_ICON_X   96
#define UI_STATUS_ICON_Y   92
#define UI_STATUS_ICON_SZ  48
#define UI_STATUS_TITLE_Y  172
#define UI_STATUS_SUB_Y    192
#define UI_STATUS_HINT_Y   212
#define UI_STATUS_BAR_X    24
#define UI_STATUS_BAR_Y    252
#define UI_STATUS_BAR_W    192
#define UI_STATUS_BAR_H    6
#define UI_STATUS_BAR_HINT 268

#define UI_SPLASH_PANEL_X  84
#define UI_SPLASH_PANEL_Y  80
#define UI_SPLASH_PANEL_W  72
#define UI_SPLASH_PANEL_H  72

#define UI_HDR_H           40
#define UI_HDR_ICON_X      8
#define UI_HDR_TEXT_X      36
#define UI_HDR_GEAR_X      212
#define UI_HDR_GEAR_Y      20
#define UI_HDR_GEAR_R      12
#define UI_HDR_GEAR_ZONE_X 188
#define UI_HDR_GEAR_ZONE_W 52

#define UI_FTR_Y           272
#define UI_FTR_H           48
#define UI_GRID_TOP        UI_HDR_H
#define UI_GRID_PAD        8
#define UI_GRID_GAP        8
#define UI_GRID_COL_W      108
#define UI_GRID_ROW_H      104
#define UI_GRID_COLS       2
#define UI_GRID_ROWS       2
#define UI_GRID_ITEMS      (UI_GRID_COLS * UI_GRID_ROWS)
#define UI_GRID_COVER_W    100
#define UI_GRID_COVER_H    68
#define UI_GRID_LABEL_MAX_W (UI_GRID_COL_W - 8)
#define UI_GRID_LABEL_Y    (4 + UI_GRID_COVER_H + 20)

#define UI_PAUSE_HDR       32
#define UI_PAUSE_ROW_H     48
#define UI_PAUSE_ICON_X    20
#define UI_PAUSE_TEXT_X    130

#define UI_SET_HDR         32
#define UI_SET_PAL_Y       32
#define UI_SET_PAL_H       60
#define UI_SET_ROW2_Y      92
#define UI_SET_ROW3_Y      144
#define UI_SET_ROW4_Y      196
#define UI_SET_ROW_H       52
#define UI_SET_SAVE_Y      248
#define UI_SET_SAVE_H      72
#define UI_SET_TAP_W       72
#define UI_SET_ICON_X      8
#define UI_SET_LABEL_X     32

#define UI_TOAST_X         20
#define UI_TOAST_Y         118
#define UI_TOAST_W         200
#define UI_TOAST_H         44

#define UI_CAL_HDR         32
#define UI_CAL_DOT_Y       52
#define UI_CAL_BAND_Y      64
#define UI_CAL_BAND_H      44
#define UI_CAL_FOOT_Y      308
#define UI_CAL_TARGET_SZ   56
