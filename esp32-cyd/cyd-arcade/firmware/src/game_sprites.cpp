#include "game_sprites.h"
#include "game_assets.h"
#include "game_play.h"
#include "ui_draw.h"
#include "ui_theme.h"
#include <Arduino.h>

void game_sprite_fill(int x, int y, int w, int h, uint16_t color) {
    game_play_fill_rect(x, y, w, h, color);
}

void game_sprite_block(int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    game_asset_draw(x + w / 2 - 8, y + h / 2 - 8, GAME_SPRITE_SNAKE, color, ui_theme_get()->play_bg);
}

void game_sprite_apple(int cx, int cy, int r) {
    (void)r;
    game_asset_draw(cx - 8, cy - 8, GAME_SPRITE_APPLE, ui_rgb565(0xFF4757), ui_theme_get()->play_bg);
}

void game_sprite_erase_apple(int cx, int cy, int r, uint16_t bg) {
    (void)r;
    game_asset_erase(cx - 8, cy - 8, 16, 16, bg);
}

void game_sprite_bird(int cx, int cy, int r, uint16_t body) {
    (void)r;
    game_asset_draw(cx - 8, cy - 8, GAME_SPRITE_BIRD, body, ui_theme_get()->play_bg);
}

void game_sprite_erase_bird(int cx, int cy, int r, uint16_t bg) {
    (void)r;
    game_asset_erase(cx - 8, cy - 8, 16, 16, bg);
}

void game_sprite_brick(int x, int y, int w, int h, uint16_t color) {
    game_asset_draw(x, y, GAME_SPRITE_BRICK, color, ui_theme_get()->play_bg);
    if (w > 16) {
        for (int dx = 16; dx < w - 8; dx += 14)
            game_asset_draw(x + dx, y, GAME_SPRITE_BRICK, color, ui_theme_get()->play_bg);
    }
}

void game_sprite_snake_head(int x, int y, int w, int h, int8_t dir, uint16_t color) {
    (void)dir;
    (void)w;
    (void)h;
    game_asset_draw(x + 2, y + 2, GAME_SPRITE_SNAKE, ui_tint565(color, 30), ui_theme_get()->play_bg);
}

void game_sprite_snake_body(int x, int y, int w, int h, uint16_t color) {
    (void)w;
    (void)h;
    game_asset_draw(x + 2, y + 2, GAME_SPRITE_SNAKE, color, ui_theme_get()->play_bg);
}

void game_sprite_ball(int cx, int cy, uint16_t color) {
    game_asset_draw(cx - 8, cy - 8, GAME_SPRITE_BALL, color, ui_theme_get()->play_bg);
}

void game_sprite_erase_ball(int cx, int cy, uint16_t bg) {
    game_asset_erase(cx - 8, cy - 8, 16, 16, bg);
}
