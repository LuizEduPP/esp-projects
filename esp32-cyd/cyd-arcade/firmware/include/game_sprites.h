#pragma once
#include <stdint.h>

void game_sprite_fill(int x, int y, int w, int h, uint16_t color);
void game_sprite_block(int x, int y, int w, int h, uint16_t color);
void game_sprite_apple(int cx, int cy, int r);
void game_sprite_erase_apple(int cx, int cy, int r, uint16_t bg);
void game_sprite_bird(int cx, int cy, int r, uint16_t body);
void game_sprite_erase_bird(int cx, int cy, int r, uint16_t bg);
void game_sprite_brick(int x, int y, int w, int h, uint16_t color);
void game_sprite_snake_head(int x, int y, int w, int h, int8_t dir, uint16_t color);
void game_sprite_snake_body(int x, int y, int w, int h, uint16_t color);
void game_sprite_ball(int cx, int cy, uint16_t color);
void game_sprite_erase_ball(int cx, int cy, uint16_t bg);
