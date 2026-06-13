#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ui_keyboard.h"

typedef enum {
    GAME_END_MENU = 0,
    GAME_END_RETRY,
} GameEndAction;

typedef struct {
    char title[24];
    char engine[16];
    char best_name[SCORE_NAME_LEN + 1];
    int score;
    int best;
    int level;
    bool quit;
    bool resume_redraw;
    uint32_t pause_after_ms;
    uint16_t accent_color;
    char level_prefix;
} GameHud;

GameHud* game_hud_begin(const char* title, const char* engine, uint32_t color);
void game_hud_end(GameHud* hud);
void game_hud_set_score(GameHud* hud, int score);
void game_hud_set_level(GameHud* hud, int level);
void game_hud_set_level_prefix(GameHud* hud, char prefix);
void game_play_toast(const char* title, const char* sub, uint16_t stroke, uint16_t bg);
void game_hud_reset_play(GameHud* hud);
bool game_hud_poll(GameHud* hud);
bool game_hud_consume_resume_redraw(GameHud* hud);
GameEndAction game_hud_end_game(GameHud* hud, int score, bool won);

void game_play_clear(uint16_t color);
void game_play_fill_rect(int x, int y, int w, int h, uint16_t color);
void game_play_fill_round_rect(int x, int y, int w, int h, int r, uint16_t color);
void game_play_fill_circle(int cx, int cy, int r, uint16_t color);
void game_play_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color);
void game_play_hint(const char* msg, uint16_t fg, uint16_t bg);
void game_play_clear_hint(uint16_t bg);
