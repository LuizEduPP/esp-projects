#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "hw_config.h"
#include <Arduino.h>
#include <stdio.h>

#define GW 20
#define CELL (PLAY_W / GW)
#define GH (PLAY_H / CELL)

static const uint16_t COL_BG    = 0x0000;
static const uint16_t COL_BODY  = 0x9D86;
static const uint16_t COL_HEAD  = 0xBEE7;
static const uint16_t COL_APPLE = 0xF800;
static const uint16_t COL_STAR  = 0xFFE0;

static int8_t body_x[GW * GH];
static int8_t body_y[GW * GH];
static int len;
static int8_t dir, ndir;
static int8_t food_x, food_y;
static bool food_star;
static uint32_t last_step;
static uint32_t step_ms;
static int score;
static int phase;
static int lives;
static uint32_t base_step_ms;

static void spawn_food();

static void snake_reset_pos() {
    len = 3;
    dir = ndir = 1;
    body_x[0] = GW / 2; body_y[0] = GH / 2;
    body_x[1] = body_x[0] - 1; body_y[1] = body_y[0];
    body_x[2] = body_x[0] - 2; body_y[2] = body_y[0];
    spawn_food();
}

static void spawn_food() {
    if (len >= GW * GH) return;

    for (int t = 0; t < 400; t++) {
        food_x = random(0, GW);
        food_y = random(0, GH);
        bool hit = false;
        for (int i = 0; i < len; i++)
            if (body_x[i] == food_x && body_y[i] == food_y) hit = true;
        if (!hit) {
            food_star = random(0, 100) < 16;
            return;
        }
    }

    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            bool hit = false;
            for (int i = 0; i < len; i++)
                if (body_x[i] == x && body_y[i] == y) hit = true;
            if (!hit) {
                food_x = (int8_t)x;
                food_y = (int8_t)y;
                food_star = random(0, 100) < 16;
                return;
            }
        }
    }
}

static void draw_seg(int x, int y, uint16_t col, bool head) {
    const int cx = x * CELL + CELL / 2;
    const int cy = y * CELL + CELL / 2;
    const int r = head ? CELL / 2 - 1 : CELL / 2 - 2;
    game_play_fill_circle(cx, cy, r, col);
    if (head) {
        game_play_fill_circle(cx - 3, cy - 2, 2, 0x0000);
        game_play_fill_circle(cx + 3, cy - 2, 2, 0x0000);
        game_play_fill_circle(cx - 3, cy - 2, 1, 0xFFFF);
        game_play_fill_circle(cx + 3, cy - 2, 1, 0xFFFF);
    }
}

static void draw_body_link(int x0, int y0, int x1, int y1, uint16_t col) {
    const int cx0 = x0 * CELL + CELL / 2;
    const int cy0 = y0 * CELL + CELL / 2;
    const int cx1 = x1 * CELL + CELL / 2;
    const int cy1 = y1 * CELL + CELL / 2;
    const int r = CELL / 2 - 2;
    game_play_fill_circle((cx0 + cx1) / 2, (cy0 + cy1) / 2, r, col);
}

static void draw_apple() {
    const int cx = food_x * CELL + CELL / 2;
    const int cy = food_y * CELL + CELL / 2;
    const uint16_t col = food_star ? COL_STAR : COL_APPLE;
    game_play_fill_circle(cx, cy, CELL / 2 - 2, col);
    if (food_star) {
        game_play_fill_circle(cx - 3, cy - 3, 2, 0xFFFF);
        game_play_fill_circle(cx + 3, cy + 3, 2, 0xFFFF);
    }
}

static void clear_cell(int x, int y) {
    game_play_fill_rect(x * CELL, y * CELL, CELL, CELL, COL_BG);
}

static void snake_redraw_all() {
    game_play_clear(COL_BG);
    for (int i = len - 1; i >= 1; i--)
        draw_body_link(body_x[i], body_y[i], body_x[i - 1], body_y[i - 1], COL_BODY);
    for (int i = len - 1; i >= 0; i--)
        draw_seg(body_x[i], body_y[i], i == 0 ? COL_HEAD : COL_BODY, i == 0);
    draw_apple();
}

static void set_dir(int sh, int sv) {
    if (sh > 0 && dir != 2) ndir = 1;
    else if (sh < 0 && dir != 1) ndir = 2;
    else if (sv > 0 && dir != 0) ndir = 3;
    else if (sv < 0 && dir != 3) ndir = 0;
}

static void handle_drag(GameInput* in, GameDrag* drag) {
    if (in->just_pressed && in->y >= PLAY_Y)
        game_drag_begin(drag, in);
    if (!drag->active) return;
    if (in->down) {
        game_drag_update(drag, in);
        const int sh = game_drag_step_h(drag, in, 16);
        const int sv = game_drag_step_v(drag, in, 16);
        if (sh) set_dir(sh, 0);
        else if (sv) set_dir(0, sv);
    }
    if (in->just_released) {
        const int sh = game_drag_swipe_h(drag);
        const int sv = game_drag_swipe_v(drag);
        if (sh) set_dir(sh, 0);
        else if (sv) set_dir(0, sv);
        drag->active = false;
    }
}

static bool snake_lose_life(GameHud* hud) {
    lives--;
    buzzer_play(SFX_ERROR);
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    if (lives <= 0) return false;
    step_ms = base_step_ms;
    snake_reset_pos();
    snake_redraw_all();
    return true;
}
static void snake_init(const GameEntry* cfg, GameHud* hud) {
    score = 0;
    phase = 1;
    lives = GAME_LIVES_DEFAULT;
    snake_reset_pos();
    last_step = millis();
    base_step_ms = cfg->speed > 0 ? cfg->speed : 150;
    step_ms = base_step_ms;
    snake_redraw_all();
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
}

void game_snake_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        snake_init(cfg, hud);
        game_hud_set_score(hud, 0);
        game_hud_set_tier(hud, phase);

        GameInput in;
        GameDrag drag = {};
        bool dead = false;

        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                snake_redraw_all();
            handle_drag(&in, &drag);

            if (millis() - last_step >= step_ms) {
                last_step = millis();
                dir = ndir;

                const int8_t tail_x = body_x[len - 1];
                const int8_t tail_y = body_y[len - 1];
                int8_t hx = body_x[0], hy = body_y[0];
                if (dir == 0) hy--;
                else if (dir == 1) hx++;
                else if (dir == 2) hx--;
                else hy++;

                bool crashed = (hx < 0 || hx >= GW || hy < 0 || hy >= GH);
                if (!crashed) {
                    for (int i = 0; i < len; i++)
                        if (body_x[i] == hx && body_y[i] == hy) {
                            crashed = true;
                            break;
                        }
                }
                if (crashed) {
                    if (!snake_lose_life(hud)) {
                        dead = true;
                        break;
                    }
                    continue;
                }

                const bool ate = (hx == food_x && hy == food_y);
                if (!ate) clear_cell(tail_x, tail_y);

                for (int i = len - 1; i > 0; i--) {
                    body_x[i] = body_x[i - 1];
                    body_y[i] = body_y[i - 1];
                }
                body_x[0] = hx;
                body_y[0] = hy;

                if (len > 1) {
                    draw_body_link(body_x[0], body_y[0], body_x[1], body_y[1], COL_BODY);
                    draw_seg(body_x[1], body_y[1], COL_BODY, false);
                }
                draw_seg(hx, hy, COL_HEAD, true);

                if (ate) {
                    len++;
                    body_x[len - 1] = tail_x;
                    body_y[len - 1] = tail_y;
                    if (food_star) {
                        len++;
                        body_x[len - 1] = tail_x;
                        body_y[len - 1] = tail_y;
                        score += 30;
                    } else {
                        score += 10;
                    }
                    if (len > 1)
                        draw_body_link(tail_x, tail_y, body_x[len - 2], body_y[len - 2], COL_BODY);
                    draw_seg(tail_x, tail_y, COL_BODY, false);
                    buzzer_play(food_star ? SFX_RECORD : SFX_SCORE);
                    const int new_phase = score / 60 + 1;
                    if (new_phase != phase) {
                        phase = new_phase;
                        if (game_hud_advance_tier(hud, phase))
                            snake_redraw_all();
                        if (step_ms > 85) step_ms -= 5;
                    }
                    spawn_food();
                    draw_apple();
                }
                game_hud_set_score(hud, score);
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
