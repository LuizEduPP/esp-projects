#pragma once
#include <stdint.h>

struct ArcadeTheme {
    uint16_t bg;
    uint16_t surface;
    uint16_t card;
    uint16_t border;
    uint16_t accent;
    uint16_t accent_hi;
    uint16_t text_hi;
    uint16_t text_mute;
    uint16_t icon;
    uint16_t play_bg;
    uint16_t ok;
    uint16_t danger;
    uint16_t pal[3];
};

#define UI_THEME_BRICK_COUNT 5

void ui_theme_init();
const ArcadeTheme* ui_theme_get();
uint16_t ui_rgb565(uint32_t rgb);
uint16_t ui_theme_brick_color(int index);
