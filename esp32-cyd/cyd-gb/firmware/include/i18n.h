#pragma once
#include <stdint.h>

#define LANG_PT  0
#define LANG_EN  1
#define LANG_ES  2
#define LANG_COUNT 3

typedef enum {
    STR_GAMES,
    STR_ROMS_FMT,
    STR_NO_ROMS,
    STR_NO_ROMS_HINT,
    STR_PREV,
    STR_NEXT,
    STR_PAUSED,
    STR_MENU_RESUME,
    STR_MENU_SAVE,
    STR_MENU_LOAD,
    STR_MENU_SETTINGS,
    STR_MENU_CALIBRATE,
    STR_MENU_QUIT,
    STR_SETTINGS,
    STR_PALETTE,
    STR_FRAME_SKIP,
    STR_BRIGHTNESS,
    STR_LANGUAGE,
    STR_SAVE_BACK,
    STR_SAVED,
    STR_LOADED,
    STR_LOADING,
    STR_OPEN_FAILED,
    STR_INIT_FAILED,
    STR_SD_ERROR,
    STR_SD_HINT,
    STR_SPLASH_SUB,
    STR_CAL_TITLE,
    STR_CAL_HINT,
    STR_CAL_TL,
    STR_CAL_TR,
    STR_CAL_BL,
    STR_CAL_BR,
    STR_CAL_CENTER,
    STR_CAL_STEP_FMT,
    STR_CAL_SAVED,
    STR_CAL_FAILED,
    STR_CAL_FACTORY,
    STR_LANG_PT,
    STR_LANG_EN,
    STR_LANG_ES,
    STR_COUNT
} StringId;

void i18n_set_lang(uint8_t lang);
uint8_t i18n_get_lang();
const char* tr(StringId id);
const char* i18n_lang_label(uint8_t lang);
