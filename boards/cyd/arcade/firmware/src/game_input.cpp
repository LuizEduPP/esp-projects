#include "game_input.h"
#include "touch_input.h"
#include <Arduino.h>

static bool s_was_down;
static bool s_allow_draw = true;

bool game_frame_tick(void) {
#if GAME_FRAME_SKIP == 0
    s_allow_draw = true;
#else
    static uint8_t ctr;
    ctr++;
    s_allow_draw = (ctr % (GAME_FRAME_SKIP + 1)) == 0;
#endif
    return s_allow_draw;
}

void game_frame_draw_now(void) {
    s_allow_draw = true;
}

bool game_frame_draw_on(void) {
    return s_allow_draw;
}

void game_frame_delay(void) {
    delay(GAME_FRAME_MS);
}

void game_input_poll(GameInput* in) {
    touch_poll();
    in->down = touch_is_pressed();
    in->just_pressed = in->down && !s_was_down;
    in->just_released = !in->down && s_was_down;
    in->x = touch_get_x();
    in->y = touch_get_y();
    in->play_x = touch_play_x();
    in->play_y = touch_play_y();
    s_was_down = in->down;
}

void game_drag_begin(GameDrag* d, const GameInput* in) {
    d->start_x = in->play_x;
    d->start_y = in->play_y;
    d->anchor_x = in->play_x;
    d->anchor_y = in->play_y;
    d->total_dx = 0;
    d->total_dy = 0;
    d->active = true;
}

void game_drag_update(GameDrag* d, const GameInput* in) {
    if (!d->active || !in->down) return;
    d->total_dx = in->play_x - d->start_x;
    d->total_dy = in->play_y - d->start_y;
}

int game_drag_swipe_h(const GameDrag* d) {
    const int thresh = 20;
    if (abs(d->total_dx) < thresh || abs(d->total_dx) < abs(d->total_dy)) return 0;
    return d->total_dx > 0 ? 1 : -1;
}

int game_drag_swipe_v(const GameDrag* d) {
    const int thresh = 20;
    if (abs(d->total_dy) < thresh || abs(d->total_dy) < abs(d->total_dx)) return 0;
    return d->total_dy > 0 ? 1 : -1;
}

int game_drag_step_h(GameDrag* d, const GameInput* in, int thresh) {
    if (!d->active || !in->down) return 0;
    const int dx = in->play_x - d->anchor_x;
    if (abs(dx) < thresh) return 0;
    d->anchor_x = in->play_x;
    return dx > 0 ? 1 : -1;
}

int game_drag_step_v(GameDrag* d, const GameInput* in, int thresh) {
    if (!d->active || !in->down) return 0;
    const int dy = in->play_y - d->anchor_y;
    if (abs(dy) < thresh) return 0;
    d->anchor_y = in->play_y;
    return dy > 0 ? 1 : -1;
}
