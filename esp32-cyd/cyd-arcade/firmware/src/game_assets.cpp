#include "game_assets.h"
#include "game_sprite_data.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>

static const uint8_t* const SPR_ALPHA[GAME_SPRITE_COUNT] = {
    SPR_BIRD_A, SPR_APPLE_A, SPR_SNAKE_A, SPR_BALL_A, SPR_BRICK_A,
};

static uint8_t alpha_at(const uint8_t* alpha, int px, int py) {
    if (px < 0 || py < 0 || px >= GAME_SPRITE_SIZE || py >= GAME_SPRITE_SIZE) return 0;
    return pgm_read_byte(&alpha[py * GAME_SPRITE_SIZE + px]);
}

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t a) {
    if (a >= 250) return fg;
    if (a <= 5) return bg;
    const uint8_t fr = (fg >> 11) & 0x1F, fg_g = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    const uint8_t br = (bg >> 11) & 0x1F, bg_g = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    const uint8_t r = (fr * a + br * (255 - a)) / 255;
    const uint8_t g = (fg_g * a + bg_g * (255 - a)) / 255;
    const uint8_t b = (fb * a + bb * (255 - a)) / 255;
    return tft.color565(r << 3, g << 2, b << 3);
}

int game_asset_w(GameSpriteId id) {
    (void)id;
    return GAME_SPRITE_SIZE;
}

int game_asset_h(GameSpriteId id) {
    if (id == GAME_SPRITE_BRICK) return 10;
    return GAME_SPRITE_SIZE;
}

void game_asset_erase(int play_x, int play_y, int w, int h, uint16_t bg) {
    tft.fillRect(PLAY_X + play_x, PLAY_Y + play_y, w, h, bg);
}

void game_asset_draw(int play_x, int play_y, GameSpriteId id, uint16_t color, uint16_t bg) {
    if (id >= GAME_SPRITE_COUNT) return;
    const uint8_t* alpha = SPR_ALPHA[id];
    const int h = game_asset_h(id);
    const int sx = PLAY_X + play_x;
    const int sy = PLAY_Y + play_y;
    for (int py = 0; py < h; py++) {
        for (int px = 0; px < GAME_SPRITE_SIZE; px++) {
            const uint8_t a = alpha_at(alpha, px, py);
            if (a < 8) continue;
            const uint16_t col = blend565(color, bg, a);
            tft.drawPixel(sx + px, sy + py, col);
        }
    }
}
