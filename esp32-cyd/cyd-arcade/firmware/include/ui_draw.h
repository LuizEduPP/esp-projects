#pragma once
#include <stdint.h>

uint16_t ui_shade565(uint32_t rgb, int8_t pct);
uint16_t ui_tint565(uint16_t color, int8_t pct);
void ui_draw_grid_icon(int x, int y, int size, uint16_t color, uint16_t bg);
void ui_fill_screen_bg();
void ui_draw_splash(const char* title, const char* subtitle);
