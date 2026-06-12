#pragma once
#include <stdint.h>

typedef struct {
    uint16_t bg;
    uint16_t surface;
    uint16_t border;
    uint16_t accent;
    uint16_t mute;
    uint16_t card;
    uint16_t card_sel;
    uint16_t btn_a;
    uint16_t btn_b;
    uint16_t pause;
    uint16_t pill;
    uint16_t stick_base;
    uint16_t stick_ring;
    uint16_t stick_knob;
    uint16_t stick_act;
    uint16_t text_hi;
    uint16_t text_lo;
    uint16_t text_mute;
    uint16_t menu_primary;
    uint16_t menu_danger;
} UiTheme;

void ui_theme_apply(uint8_t palette_idx);
const UiTheme* ui_theme_get();
