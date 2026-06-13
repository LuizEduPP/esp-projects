#pragma once
#include <stdint.h>

typedef enum {
    GAME_SPRITE_BIRD,
    GAME_SPRITE_APPLE,
    GAME_SPRITE_SNAKE,
    GAME_SPRITE_BALL,
    GAME_SPRITE_BRICK,
    GAME_SPRITE_COUNT,
} GameSpriteId;

void game_asset_draw(int play_x, int play_y, GameSpriteId id, uint16_t color, uint16_t bg);
void game_asset_erase(int play_x, int play_y, int w, int h, uint16_t bg);
int game_asset_w(GameSpriteId id);
int game_asset_h(GameSpriteId id);
