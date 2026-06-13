#pragma once
#include <stdint.h>

void game_gfx_block(int x, int y, int w, int h, int r, uint16_t color, uint16_t shadow);
void game_gfx_circle(int cx, int cy, int r, uint16_t color, uint16_t shadow);
