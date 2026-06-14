#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "hw_config.h"
#include <Arduino.h>
#include <math.h>

#define PAD_W   70
#define PAD_H   12
#define BALL_R  7
#define PHYS_MS 18
#define FLOOR_H 4

static const uint16_t COL_BG    = 0x0000;
static const uint16_t COL_FLOOR = 0x2949;
static const uint16_t COL_PAD   = 0xFFFF;
static const uint16_t COL_BALL  = 0xFFE0;
static const uint16_t COL_LINE  = 0x4208;

static int pad_x;
static float bx, by, bdx, bdy;
static int score, lives, last_hud_score, last_hud_lives;
static bool serving;
static uint32_t last_phys, serve_until;
static int prev_bx, prev_by, prev_pad;
static bool ball_on;

static int pad_y() { return PLAY_H - PAD_H - 16; }
static int floor_y() { return PLAY_H - FLOOR_H; }

static void normalize(float spd) {
    const float len = sqrtf(bdx * bdx + bdy * bdy);
    if (len < 0.01f) return;
    bdx = bdx / len * spd;
    bdy = bdy / len * spd;
}

static void draw_dash_line() {
    for (int y = 12; y < pad_y(); y += 14)
        game_play_fill_rect(PLAY_W / 2 - 1, y, 2, 7, COL_LINE);
}

static void restore_dash_in_rect(int x, int y, int w, int h) {
    const int cx = PLAY_W / 2 - 1;
    if (x + w < cx || x > cx + 1) return;
    for (int ly = 12; ly < pad_y(); ly += 14) {
        if (ly + 7 >= y && ly <= y + h)
            game_play_fill_rect(cx, ly, 2, 7, COL_LINE);
    }
}

static void restore_floor_in_rect(int x, int y, int w, int h) {
    const int ly = floor_y();
    if (y + h < ly || y > ly + FLOOR_H) return;
    const int fx = max(0, x);
    const int fw = min(x + w, PLAY_W) - fx;
    if (fw > 0)
        game_play_fill_rect(fx, ly, fw, FLOOR_H, COL_FLOOR);
}

static void draw_bg_static() {
    game_play_clear(COL_BG);
    draw_dash_line();
    game_play_fill_rect(0, floor_y(), PLAY_W, FLOOR_H, COL_FLOOR);
}

static void draw_pad(int x) {
    game_play_fill_round_rect(x - PAD_W / 2, pad_y(), PAD_W, PAD_H, 6, COL_PAD);
}

static void erase_pad(int x) {
    const int ex = x - PAD_W / 2 - 1;
    const int ey = pad_y() - 1;
    const int ew = PAD_W + 2;
    const int eh = PAD_H + 2;
    game_play_fill_rect(ex, ey, ew, eh, COL_BG);
    restore_dash_in_rect(ex, ey, ew, eh);
    restore_floor_in_rect(ex, ey, ew, eh);
}

static void draw_ball(int x, int y) {
    game_play_fill_circle(x, y, BALL_R, COL_BALL);
}

static void erase_ball(int x, int y) {
    const int ex = x - BALL_R - 1;
    const int ey = y - BALL_R - 1;
    const int ew = BALL_R * 2 + 2;
    const int eh = BALL_R * 2 + 2;
    game_play_fill_rect(ex, ey, ew, eh, COL_BG);
    restore_dash_in_rect(ex, ey, ew, eh);
    restore_floor_in_rect(ex, ey, ew, eh);
}

static void sync_ball_pos() {
    if (serving) {
        bx = (float)pad_x;
        by = (float)(pad_y() - BALL_R - 2);
    }
}

static void sync_ball_draw() {
    sync_ball_pos();
    const int nbx = (int)bx, nby = (int)by;
    if (nbx == prev_bx && nby == prev_by) return;
    if (ball_on)
        erase_ball(prev_bx, prev_by);
    draw_ball(nbx, nby);
    prev_bx = nbx;
    prev_by = nby;
    ball_on = true;
}

static void serve_ball() {
    serving = true;
    serve_until = millis() + 400;
    bx = (float)pad_x;
    by = (float)(pad_y() - BALL_R - 2);
    bdx = (random(0, 2) ? 1.0f : -1.0f) * 1.8f;
    bdy = -2.4f;
    normalize(2.6f);
    ball_on = false;
    sync_ball_draw();
}

static void bounce_redraw() {
    draw_bg_static();
    draw_pad(pad_x);
    ball_on = false;
    sync_ball_draw();
}

static void bounce_init() {
    pad_x = PLAY_W / 2;
    score = 0;
    lives = 3;
    last_hud_score = -1;
    last_hud_lives = -1;
    prev_pad = pad_x;
    draw_bg_static();
    draw_pad(pad_x);
    serve_ball();
    last_phys = millis();
}

static bool physics_step() {
    bx += bdx;
    by += bdy;

    if (bx < BALL_R) { bx = BALL_R; bdx = fabsf(bdx); }
    if (bx > PLAY_W - BALL_R) { bx = PLAY_W - BALL_R; bdx = -fabsf(bdx); }
    if (by < BALL_R + 4) { by = BALL_R + 4; bdy = fabsf(bdy); }

    const int py = pad_y();
    const int pad_bot = py + PAD_H;

    if (bdy > 0 && by + BALL_R >= py && by - BALL_R <= pad_bot &&
        bx >= pad_x - PAD_W / 2 - 4 && bx <= pad_x + PAD_W / 2 + 4) {
        by = (float)(py - BALL_R);
        bdy = -fabsf(bdy);
        const float hit = (bx - pad_x) / (PAD_W * 0.45f);
        bdx = hit * 2.8f;
        normalize(2.6f + min(score / 40, 2));
        score += 10;
        buzzer_play(SFX_HIT);
        return true;
    }

    if (by - BALL_R > pad_bot) {
        lives--;
        buzzer_play(SFX_ERROR);
        if (lives <= 0) return false;
        erase_ball(prev_bx, prev_by);
        ball_on = false;
        serve_ball();
    }
    return true;
}

void game_bounce_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->title, cfg->engine, cfg->color);
    if (!hud) return;
    game_hud_set_level_prefix(hud, 'V');

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        bounce_init();
        game_hud_set_score(hud, 0);
        game_hud_set_level(hud, lives);
        last_hud_score = 0;
        last_hud_lives = lives;

        GameInput in;
        bool dead = false;

        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                bounce_redraw();

            if (in.down && in.y >= PLAY_Y)
                pad_x = constrain((int)in.play_x, PAD_W / 2, PLAY_W - PAD_W / 2);

            if (serving) {
                if (in.just_pressed && in.y >= PLAY_Y) {
                    serving = false;
                    last_phys = millis();
                } else if (millis() >= serve_until) {
                    serving = false;
                    last_phys = millis();
                }
            } else if (millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                if (!physics_step()) dead = true;
            }

            if (prev_pad != pad_x) {
                erase_pad(prev_pad);
                draw_pad(pad_x);
                prev_pad = pad_x;
            }

            sync_ball_draw();

            if (score != last_hud_score) {
                game_hud_set_score(hud, score);
                last_hud_score = score;
            }
            if (lives != last_hud_lives) {
                game_hud_set_level(hud, lives);
                last_hud_lives = lives;
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
