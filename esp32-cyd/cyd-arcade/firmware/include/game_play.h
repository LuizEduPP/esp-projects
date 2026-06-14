#pragma once
#include <stdint.h>
#include <stdbool.h>

#include "ui_keyboard.h"

typedef enum {
    GAME_END_MENU = 0,
    GAME_END_RETRY,
} GameEndAction;

#define GAME_LIVES_DEFAULT 3

typedef enum {
    HUD_TIER_NONE = 0,
    HUD_TIER_FASE,
    HUD_TIER_NIVEL,
    HUD_TIER_CPU,
} HudTierKind;

typedef struct {
    char engine[16];
    char best_name[SCORE_NAME_LEN + 1];
    char score_tag[8];
    int score;
    int best;
    int tier;
    int lives;
    int lives_max;
    bool quit;
    bool resume_redraw;
    bool tier_show_zero;
    bool score_lower_better;
    uint32_t pause_after_ms;
    uint32_t toast_until_ms;
    int16_t toast_x;
    int16_t toast_y;
    int16_t toast_w;
    int16_t toast_h;
    HudTierKind tier_kind;
} GameHud;

GameHud* game_hud_begin(const char* engine);
void game_hud_end(GameHud* hud);
void game_hud_set_score(GameHud* hud, int score);
void game_hud_set_score_tag(GameHud* hud, const char* tag);
void game_hud_set_tier_mode(GameHud* hud, HudTierKind kind, bool show_at_zero);
void game_hud_set_tier(GameHud* hud, int value);
bool game_hud_advance_tier(GameHud* hud, int new_tier);
void game_hud_set_lives(GameHud* hud, int lives, int max_lives);
void game_play_toast(const char* title, const char* sub, uint16_t stroke, uint16_t bg);
void game_hud_reset_play(GameHud* hud);
bool game_hud_poll(GameHud* hud);
bool game_hud_consume_resume_redraw(GameHud* hud);
GameEndAction game_hud_end_game(GameHud* hud, int score, bool won);

void game_play_clear(uint16_t color);
void game_play_fill_rect(int x, int y, int w, int h, uint16_t color);
void game_play_fill_round_rect(int x, int y, int w, int h, int r, uint16_t color);
void game_play_fill_circle(int cx, int cy, int r, uint16_t color);
void game_play_hint(const char* msg, uint16_t fg, uint16_t bg);
void game_play_clear_hint(uint16_t bg);

uint16_t game_play_field_bg(void);
