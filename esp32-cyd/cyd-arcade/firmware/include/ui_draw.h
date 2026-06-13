#pragma once
#include <stdint.h>

uint16_t ui_shade565(uint32_t rgb, int8_t pct);
uint16_t ui_tint565(uint16_t color, int8_t pct);
void ui_draw_grid_icon(int x, int y, int size, uint16_t color, uint16_t bg);
void ui_draw_gear(int x, int y, int size, uint16_t color, uint16_t bg);
void ui_draw_chevron(int x, int y, int size, bool right, uint16_t color);
void ui_draw_nav_btn(int cx, int cy, int r, bool right, bool enabled);
void ui_draw_score_badge(int cx, int y, const char* text, uint16_t fg, uint16_t bg);
void ui_draw_pause_btn(int x, int y, int w, int h, uint16_t fg, uint16_t bg, uint16_t border);
void ui_draw_game_cover(const char* engine, int x, int y, int w, int h, int brick_idx);
void ui_fill_screen_bg();
void ui_draw_splash(const char* title, const char* subtitle);
