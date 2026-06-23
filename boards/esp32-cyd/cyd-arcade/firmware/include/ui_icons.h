#pragma once
#include <stdint.h>

typedef enum {
    UI_ICON_GB,
    UI_ICON_SD,
    UI_ICON_CART,
    UI_ICON_FOLDER,
    UI_ICON_GEAR,
    UI_ICON_GRID,
    UI_ICON_CHEV_L,
    UI_ICON_CHEV_R,
    UI_ICON_PAUSE,
    UI_ICON_PLAY,
    UI_ICON_SAVE,
    UI_ICON_LOAD,
    UI_ICON_SLIDERS,
    UI_ICON_TARGET,
    UI_ICON_EXIT,
    UI_ICON_PALETTE,
    UI_ICON_SPEED,
    UI_ICON_SUN,
    UI_ICON_GLOBE,
    UI_ICON_CHECK,
    UI_ICON_X,
    UI_ICON_SELECT,
    UI_ICON_CROSS,
    UI_ICON_COUNT
} UiIcon;

void ui_icon_draw(int x, int y, int size, UiIcon icon, uint16_t color);
void ui_icon_draw_on(int x, int y, int size, UiIcon icon, uint16_t color, uint16_t bg);
void ui_icon_draw_t(int x, int y, int size, UiIcon icon);
void ui_icon_draw_ok(int x, int y, int size, UiIcon icon);
void ui_icon_draw_danger(int x, int y, int size, UiIcon icon);
