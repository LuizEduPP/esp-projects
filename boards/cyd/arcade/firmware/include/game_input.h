#pragma once
#include <stdint.h>
#include <stdbool.h>

#define GAME_FRAME_MS   16
#define GAME_FRAME_SKIP 0  /* 0=sem skip; ghosting se >0 em jogos com redraw parcial */

bool game_frame_tick(void);
void game_frame_draw_now(void);
bool game_frame_draw_on(void);
void game_frame_delay(void);

typedef struct {
    bool down;
    bool just_pressed;
    bool just_released;
    int16_t x, y;
    int16_t play_x, play_y;
} GameInput;

typedef struct {
    int16_t start_x, start_y;
    int16_t anchor_x, anchor_y;
    int total_dx, total_dy;
    bool active;
} GameDrag;

void game_input_poll(GameInput* in);
void game_drag_begin(GameDrag* d, const GameInput* in);
void game_drag_update(GameDrag* d, const GameInput* in);
int game_drag_swipe_h(const GameDrag* d);
int game_drag_swipe_v(const GameDrag* d);
int game_drag_step_h(GameDrag* d, const GameInput* in, int thresh);
int game_drag_step_v(GameDrag* d, const GameInput* in, int thresh);
