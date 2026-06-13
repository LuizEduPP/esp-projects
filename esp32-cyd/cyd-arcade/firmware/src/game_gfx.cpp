#include "game_gfx.h"
#include "game_play.h"
#include "ui_draw.h"

void game_gfx_block(int x, int y, int w, int h, int r, uint16_t color, uint16_t shadow) {
    if (w <= 0 || h <= 0) return;
    game_play_fill_round_rect(x + 1, y + 1, w, h, r, shadow);
    game_play_fill_round_rect(x, y, w, h, r, color);
    game_play_fill_rect(x, y, w, 2, ui_tint565(color, 35));
}

void game_gfx_circle(int cx, int cy, int r, uint16_t color, uint16_t shadow) {
    if (r <= 0) return;
    game_play_fill_circle(cx + 1, cy + 1, r, shadow);
    game_play_fill_circle(cx, cy, r, color);
}
