#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "hw_config.h"
#include <Arduino.h>
#include <math.h>

#define PAD_W      56
#define PAD_H      8
#define BALL_R     5
#define PHYS_MS    16
#define BALL_SPD   2.6f
#define CPU_SPD    1.0f
#define WIN_SCORE  5

static const uint16_t COL_BG   = 0x0000;
static const uint16_t COL_LINE = 0x4208;
static const uint16_t COL_PAD  = 0xFFFF;
static const uint16_t COL_BALL = 0xFFFF;

enum { PONG_OK = 0, PONG_LOST, PONG_WON };

static int pad_p, pad_c;
static float ball_x, ball_y, ball_dx, ball_dy;
static int score, cpu_score;
static bool serving;
static uint32_t last_phys, serve_until;
static int prev_bx, prev_by, prev_px, prev_cx;
static int last_hud_cpu;

static int cpu_y() { return 14; }
static int player_y() { return PLAY_H - PAD_H - 14; }

static void draw_center_line() {
    for (int y = 10; y < PLAY_H - 10; y += 12)
        game_play_fill_rect(PLAY_W / 2 - 1, y, 2, 6, COL_LINE);
}

static void restore_dash_in_rect(int x, int y, int w, int h) {
    const int cx = PLAY_W / 2 - 1;
    if (x + w < cx || x > cx + 1) return;
    for (int ly = 10; ly < PLAY_H - 10; ly += 12) {
        if (ly + 6 >= y && ly <= y + h)
            game_play_fill_rect(cx, ly, 2, 6, COL_LINE);
    }
}

static void draw_pad(int x, int y) {
    game_play_fill_round_rect(x - PAD_W / 2, y, PAD_W, PAD_H, 4, COL_PAD);
}

static void erase_pad(int x, int y) {
    const int ex = x - PAD_W / 2 - 1;
    const int ey = y - 1;
    const int ew = PAD_W + 2;
    const int eh = PAD_H + 2;
    game_play_fill_rect(ex, ey, ew, eh, COL_BG);
    restore_dash_in_rect(ex, ey, ew, eh);
}

static void draw_ball(int bx, int by) {
    game_play_fill_circle(bx, by, BALL_R, COL_BALL);
}

static void erase_ball(int bx, int by) {
    const int ex = bx - BALL_R - 1;
    const int ey = by - BALL_R - 1;
    const int ew = BALL_R * 2 + 2;
    const int eh = BALL_R * 2 + 2;
    game_play_fill_rect(ex, ey, ew, eh, COL_BG);
    restore_dash_in_rect(ex, ey, ew, eh);
}

static void normalize_ball(float spd) {
    const float len = sqrtf(ball_dx * ball_dx + ball_dy * ball_dy);
    if (len < 0.01f) {
        ball_dx = spd;
        ball_dy = spd * 0.5f;
        return;
    }
    ball_dx = ball_dx / len * spd;
    ball_dy = ball_dy / len * spd;
}

static void pong_redraw() {
    game_play_clear(COL_BG);
    draw_center_line();
    draw_pad(pad_c, cpu_y());
    draw_pad(pad_p, player_y());
    draw_ball((int)ball_x, (int)ball_y);
    prev_bx = (int)ball_x;
    prev_by = (int)ball_y;
    prev_px = pad_p;
    prev_cx = pad_c;
}

static void serve_ball(bool toward_player) {
    ball_x = PLAY_W / 2.0f;
    ball_y = PLAY_H / 2.0f;
    const float angle = (random(-35, 36) * 3.14159f) / 180.0f;
    ball_dx = sinf(angle) * BALL_SPD;
    ball_dy = (toward_player ? 1.0f : -1.0f) * cosf(angle) * BALL_SPD;
    if (fabsf(ball_dy) < 0.8f)
        ball_dy = (toward_player ? 1.0f : -1.0f) * 0.8f;
    normalize_ball(BALL_SPD + (score / 8) * 0.1f);
    serving = true;
    serve_until = millis() + 600;
    pong_redraw();
}

static void pong_init() {
    pad_p = PLAY_W / 2;
    pad_c = PLAY_W / 2;
    score = 0;
    cpu_score = 0;
    last_hud_cpu = -1;
    last_phys = millis();
    serve_ball(true);
}

static bool paddle_hit(int pad_x, int pad_top, bool from_above) {
    const float by = ball_y;
    const float bx = ball_x;
    if (bx < pad_x - PAD_W / 2 - BALL_R || bx > pad_x + PAD_W / 2 + BALL_R)
        return false;
    if (from_above) {
        if (ball_dy > 0 && by + BALL_R >= pad_top && by < pad_top + PAD_H + BALL_R) {
            ball_y = pad_top - BALL_R;
            ball_dy = -fabsf(ball_dy);
            const float hit = (bx - pad_x) / (PAD_W * 0.5f);
            ball_dx = hit * 2.8f;
            normalize_ball(BALL_SPD + (score / 8) * 0.1f);
            buzzer_play(SFX_HIT);
            return true;
        }
    } else {
        if (ball_dy < 0 && by - BALL_R <= pad_top + PAD_H && by > pad_top - BALL_R) {
            ball_y = pad_top + PAD_H + BALL_R;
            ball_dy = fabsf(ball_dy);
            const float hit = (bx - pad_x) / (PAD_W * 0.5f);
            ball_dx = hit * 2.8f;
            normalize_ball(BALL_SPD + (score / 8) * 0.1f);
            buzzer_play(SFX_HIT);
            return true;
        }
    }
    return false;
}

static int physics_step() {
    ball_x += ball_dx;
    ball_y += ball_dy;

    if (ball_x < BALL_R) {
        ball_x = BALL_R;
        ball_dx = fabsf(ball_dx);
    } else if (ball_x > PLAY_W - BALL_R) {
        ball_x = PLAY_W - BALL_R;
        ball_dx = -fabsf(ball_dx);
    }

    paddle_hit(pad_c, cpu_y(), false);
    paddle_hit(pad_p, player_y(), true);

    if (ball_y < -BALL_R) {
        score++;
        buzzer_play(SFX_SCORE);
        if (score >= WIN_SCORE)
            return PONG_WON;
        serve_ball(true);
        return PONG_OK;
    }
    if (ball_y > PLAY_H + BALL_R) {
        cpu_score++;
        buzzer_play(SFX_ERROR);
        if (cpu_score >= WIN_SCORE)
            return PONG_LOST;
        serve_ball(false);
        return PONG_OK;
    }
    return PONG_OK;
}

static void sync_draw() {
    if (prev_px != pad_p) {
        erase_pad(prev_px, player_y());
        draw_pad(pad_p, player_y());
        prev_px = pad_p;
    }
    if (prev_cx != pad_c) {
        erase_pad(prev_cx, cpu_y());
        draw_pad(pad_c, cpu_y());
        prev_cx = pad_c;
    }

    const int bx = (int)ball_x;
    const int by = (int)ball_y;
    if (bx != prev_bx || by != prev_by) {
        if (!serving)
            erase_ball(prev_bx, prev_by);
        draw_ball(bx, by);
        prev_bx = bx;
        prev_by = by;
    }
}

void game_pong_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->title, cfg->engine, cfg->color);
    if (!hud) return;
    game_hud_set_level_prefix(hud, 'C');

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        pong_init();
        game_hud_set_score(hud, 0);
        game_hud_set_level(hud, 0);

        GameInput in;
        bool dead = false;
        bool won = false;

        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                pong_redraw();

            if (in.down && in.y >= PLAY_Y)
                pad_p = constrain((int)in.play_x, PAD_W / 2, PLAY_W - PAD_W / 2);

            if (!serving && ball_dy < 0 && ball_y < PLAY_H * 0.45f) {
                float target = ball_x;
                if (random(0, 100) < 25)
                    target += (random(0, 2) ? 1 : -1) * random(20, 50);
                if (pad_c < target - 2)
                    pad_c = (int)min((float)pad_c + CPU_SPD, target);
                else if (pad_c > target + 2)
                    pad_c = (int)max((float)pad_c - CPU_SPD, target);
            }
            pad_c = constrain(pad_c, PAD_W / 2, PLAY_W - PAD_W / 2);

            if (serving) {
                if (in.just_pressed || millis() >= serve_until) {
                    serving = false;
                    last_phys = millis();
                }
            } else if (millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                const int res = physics_step();
                if (res != PONG_OK) {
                    dead = true;
                    won = (res == PONG_WON);
                }
            }

            game_frame_draw_now();
            sync_draw();

            if (score != hud->score)
                game_hud_set_score(hud, score);
            if (cpu_score != last_hud_cpu) {
                game_hud_set_level(hud, cpu_score);
                last_hud_cpu = cpu_score;
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, won) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
