#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "hw_config.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#define TH ui_theme_get()

#define COLS 8
#define ROWS 5
#define PAD_W 64
#define PAD_H 12
#define BALL_R 5
#define LIVES_MAX GAME_LIVES_DEFAULT
#define PHYS_MS 16

static const uint16_t COL_BALL = 0xFFFF;
static const uint16_t COL_PAD  = 0xDEFB;
#define COL_BG 0x0000

static const uint16_t ROW_COLORS[ROWS] = {
    0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x001F,
};

static bool bricks[ROWS][COLS];
static int pad_x;
static float ball_x, ball_y, ball_dx, ball_dy;
static int score, lives, level;
static bool ball_live;
static bool level_cleared;
static bool hint_visible;
static uint32_t last_phys;

static int brick_w() { return PLAY_W / COLS; }
static int brick_h() { return 12 + (level > 3 ? 1 : 0); }
static int brick_top() { return PLAY_MARGIN + 8 + (level - 1) * 2; }
static int pad_y() { return PLAY_H - PLAY_MARGIN - PAD_H - 4; }

static void brick_rect(int r, int c, int* x, int* y, int* w, int* h) {
    const int bw = brick_w(), bh = brick_h();
    *x = c * bw + 2;
    *y = r * bh + brick_top();
    *w = bw - 4;
    *h = bh - 2;
}

static bool rects_overlap(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static bool bricks_left() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (bricks[r][c]) return true;
    return false;
}

static void init_level() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            bricks[r][c] = true;

    if (level >= 2) {
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if ((r + c + level) % 2 == 0)
                    bricks[r][c] = false;
    }
    if (level >= 4) {
        for (int c = 0; c < COLS; c++)
            bricks[0][c] = (c % 2 == 0);
    }
    if (level >= 6) {
        for (int r = 1; r < ROWS - 1; r++)
            bricks[r][COLS / 2] = false;
    }
    if (!bricks_left()) {
        for (int c = 0; c < COLS; c++)
            bricks[ROWS / 2][c] = true;
    }

    pad_x = PLAY_W / 2;
    ball_x = (float)pad_x;
    ball_y = (float)(pad_y() - BALL_R - 4);
    const float spd = 2.1f + level * 0.20f;
    ball_dx = spd * 0.6f;
    ball_dy = -spd;
    ball_live = false;
    hint_visible = false;
}

static void draw_brick(int r, int c) {
    if (!bricks[r][c]) return;
    int x, y, w, h;
    brick_rect(r, c, &x, &y, &w, &h);
    game_play_fill_round_rect(x, y, w, h, 2, ROW_COLORS[r]);
}

static void redraw_bricks_in_rect(int rx, int ry, int rw, int rh) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!bricks[r][c]) continue;
            int bx, by, bw, bh;
            brick_rect(r, c, &bx, &by, &bw, &bh);
            if (rects_overlap(rx, ry, rw, rh, bx, by, bw, bh))
                draw_brick(r, c);
        }
    }
}

static void draw_all_bricks() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            draw_brick(r, c);
}

static void draw_pad(int px) {
    game_play_fill_round_rect(px - PAD_W / 2, pad_y(), PAD_W, PAD_H, 5, COL_PAD);
}

static void draw_ball(int bx, int by) {
    game_play_fill_circle(bx, by, BALL_R, COL_BALL);
}

static void erase_rect(int ex, int ey, int ew, int eh) {
    game_play_fill_rect(ex, ey, ew, eh, COL_BG);
    redraw_bricks_in_rect(ex, ey, ew, eh);
}

static void erase_ball_at(int bx, int by) {
    const int m = BALL_R + 1;
    erase_rect(bx - m, by - m, m * 2, m * 2);
}

static void erase_pad_at(int px) {
    erase_rect(px - PAD_W / 2 - 1, pad_y() - 1, PAD_W + 2, PAD_H + 2);
}

static void breakout_redraw(GameHud* hud) {
    game_play_clear(COL_BG);
    draw_all_bricks();
    draw_pad(pad_x);
    draw_ball((int)ball_x, (int)ball_y);
    (void)hud;
}

static void launch_ball() {
    ball_live = true;
    hint_visible = false;
    game_play_clear_hint(COL_BG);
    ball_x = (float)pad_x;
    ball_y = (float)(pad_y() - BALL_R - 4);
    const float angle = random(-20, 21) * 0.01745f;
    const float spd = 2.1f + level * 0.20f;
    ball_dx = sinf(angle) * spd;
    ball_dy = -cosf(angle) * spd;
}

static bool collide_brick() {
    const int bx = (int)ball_x;
    const int by = (int)ball_y;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (!bricks[r][c]) continue;
            int rx, ry, rw, rh;
            brick_rect(r, c, &rx, &ry, &rw, &rh);
            if (bx + BALL_R <= rx || bx - BALL_R >= rx + rw ||
                by + BALL_R <= ry || by - BALL_R >= ry + rh)
                continue;

            bricks[r][c] = false;
            score += 10 + (ROWS - r) * 5;
            buzzer_play(SFX_SCORE);
            game_play_fill_rect(rx - 1, ry - 1, rw + 2, rh + 2, COL_BG);

            const int cx = rx + rw / 2;
            if (bx < cx) ball_dx = -fabsf(ball_dx);
            else ball_dx = fabsf(ball_dx);
            if (by < ry + rh / 2) ball_dy = -fabsf(ball_dy);
            else ball_dy = fabsf(ball_dy);
            return true;
        }
    }
    return false;
}

static void physics_step() {
    level_cleared = false;
    ball_x += ball_dx;
    ball_y += ball_dy;

    if (ball_x < BALL_R) { ball_x = BALL_R; ball_dx = fabsf(ball_dx); }
    if (ball_x > PLAY_W - BALL_R) { ball_x = PLAY_W - BALL_R; ball_dx = -fabsf(ball_dx); }
    if (ball_y < BALL_R) { ball_y = BALL_R; ball_dy = fabsf(ball_dy); }

    const int py = pad_y();
    if (ball_dy > 0 && ball_y + BALL_R >= py && ball_y - BALL_R <= py + PAD_H &&
        ball_x >= pad_x - PAD_W / 2 && ball_x <= pad_x + PAD_W / 2) {
        ball_y = py - BALL_R;
        ball_dy = -fabsf(ball_dy);
        buzzer_play(SFX_HIT);
        const float hit = (ball_x - pad_x) / (PAD_W * 0.5f);
        ball_dx = hit * 3.2f;
        if (fabsf(ball_dx) < 0.6f) ball_dx = ball_dx < 0 ? -0.6f : 0.6f;
        const float cap = 2.1f + level * 0.20f;
        const float cur = sqrtf(ball_dx * ball_dx + ball_dy * ball_dy);
        if (cur > cap) {
            ball_dx = ball_dx / cur * cap;
            ball_dy = ball_dy / cur * cap;
        }
    }

    if (ball_y > PLAY_H + BALL_R) {
        lives--;
        buzzer_play(SFX_ERROR);
        ball_live = false;
        ball_x = (float)pad_x;
        ball_y = (float)(pad_y() - BALL_R - 4);
        hint_visible = false;
    }

    collide_brick();

    if (ball_live && !bricks_left()) {
        level++;
        score += 80 * level;
        init_level();
        level_cleared = true;
    }
}

static bool pad_overlaps_ball() {
    const int py = pad_y();
    return ball_y + BALL_R >= py - 2 && ball_y - BALL_R <= py + PAD_H + 2;
}

static void breakout_init(GameHud* hud) {
    score = 0;
    lives = LIVES_MAX;
    level = 1;
    init_level();
    last_phys = millis();
    breakout_redraw(hud);
    game_hud_set_score(hud, 0);
    game_hud_set_tier(hud, level);
    game_hud_set_lives(hud, lives, LIVES_MAX);
}

void game_breakout_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_NIVEL, false);

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        breakout_init(hud);

        int prev_pad = pad_x;
        int prev_bx = (int)ball_x, prev_by = (int)ball_y;
        int prev_lives = lives;
        bool dead = false;

        GameInput in;
        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                breakout_redraw(hud);

            if (in.down && in.y >= PLAY_Y) {
                pad_x = constrain((int)in.play_x, PAD_W / 2, PLAY_W - PAD_W / 2);
                if (!ball_live && in.just_pressed)
                    launch_ball();
            }

            if (!ball_live) {
                ball_x = (float)pad_x;
                ball_y = (float)(pad_y() - BALL_R - 4);
                if (!hint_visible) {
                    game_play_hint("TOQUE P/ LANCAR", TH->accent, COL_BG);
                    hint_visible = true;
                }
            } else if (millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                physics_step();
                if (lives <= 0) {
                    dead = true;
                    break;
                }
                if (lives < prev_lives) {
                    game_hud_set_lives(hud, lives, LIVES_MAX);
                    prev_lives = lives;
                }
                if (level_cleared) {
                    game_hud_advance_tier(hud, level);
                    breakout_redraw(hud);
                    prev_lives = lives;
                    prev_pad = pad_x;
                    prev_bx = (int)ball_x;
                    prev_by = (int)ball_y;
                }
            }

            if (prev_pad != pad_x) {
                erase_pad_at(prev_pad);
                draw_pad(pad_x);
                prev_pad = pad_x;
            }

            const int bx = (int)ball_x, by = (int)ball_y;
            if (bx != prev_bx || by != prev_by) {
                erase_ball_at(prev_bx, prev_by);
                if (pad_overlaps_ball())
                    draw_pad(pad_x);
                draw_ball(bx, by);
                prev_bx = bx;
                prev_by = by;
            }

            if (score != hud->score)
                game_hud_set_score(hud, score);
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
