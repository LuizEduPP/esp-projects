#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "ui_draw.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define COLS     8
#define MAX_ROWS 9
#define BALL_R   5
#define LAUNCH_Y (PLAY_H - 20)
#define DANGER_R (MAX_ROWS - 2)
#define COL_BG   0x0000
#define COL_BALL 0xFFFF
#define COL_AIM  0x4208
#define PHYS_MS  16

static int8_t grid[MAX_ROWS][COLS];
static float ball_x, ball_y, ball_dx, ball_dy;
static bool ball_live;
static bool aiming;
static int aim_px, aim_py;
static int score, level, lives;
static uint32_t last_phys;
static int prev_bx, prev_by;

static int cell_w() { return PLAY_W / COLS; }
static int cell_h() { return 24; }

static uint16_t block_color(int hp) {
    static const uint16_t cols[] = {0x001F, 0x07FF, 0x07E0, 0xFFE0, 0xFD20, 0xF800, 0xF81F, 0xFFFF};
    if (hp <= 0) return COL_BG;
    if (hp > 8) return cols[7];
    return cols[hp - 1];
}

static void spawn_row(int row) {
    for (int c = 0; c < COLS; c++) {
        if (random(0, 100) < 30) {
            grid[row][c] = 0;
            continue;
        }
        int hp = random(1, 4) + level / 2;
        if (hp > 9) hp = 9;
        grid[row][c] = (int8_t)hp;
    }
}

static void shift_blocks_down() {
    for (int r = MAX_ROWS - 1; r >= 1; r--)
        memcpy(grid[r], grid[r - 1], COLS);
    spawn_row(0);
}

static bool blocks_in_danger() {
    for (int c = 0; c < COLS; c++)
        if (grid[DANGER_R][c] > 0) return true;
    return false;
}

static bool any_blocks_left() {
    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (grid[r][c] > 0) return true;
    return false;
}

static void draw_block(int c, int r) {
    const int hp = grid[r][c];
    if (hp <= 0) return;
    const int x = c * cell_w() + 2;
    const int y = r * cell_h() + 4;
    const int w = cell_w() - 4;
    const int h = cell_h() - 4;
    const uint16_t col = block_color(hp);
    game_play_fill_round_rect(x, y, w, h, 4, col);
    char s[2] = {(char)('0' + hp), 0};
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(hp >= 5 ? 0x0000 : 0xFFFF, col);
    tft.drawString(s, PLAY_X + x + w / 2, PLAY_Y + y + h / 2, 2);
}

static void draw_aim_line() {
    if (!aiming) return;
    const int cx = PLAY_W / 2;
    const int cy = LAUNCH_Y;
    const float dx = (float)(aim_px - cx);
    const float dy = (float)(aim_py - cy);
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 12.0f) return;
    const int steps = (int)(len / 8.0f);
    for (int i = 1; i <= steps; i++) {
        const int px = cx + (int)(dx * i / steps);
        const int py = cy + (int)(dy * i / steps);
        if (py < 0) break;
        game_play_fill_circle(px, py, 2, COL_AIM);
    }
}

static void ballz_redraw() {
    game_play_clear(COL_BG);
    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < COLS; c++)
            draw_block(c, r);
    game_play_fill_circle(PLAY_W / 2, LAUNCH_Y, 6, ui_tint565(0xFFFF, -40));
    if (ball_live)
        game_play_fill_circle((int)ball_x, (int)ball_y, BALL_R, COL_BALL);
    draw_aim_line();
    prev_bx = (int)ball_x;
    prev_by = (int)ball_y;
}

static void launch_ball() {
    const int cx = PLAY_W / 2;
    const int cy = LAUNCH_Y;
    float dx = (float)(aim_px - cx);
    float dy = (float)(aim_py - cy);
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 12.0f) {
        dx = 0.0f;
        dy = -1.0f;
    } else {
        dx /= len;
        dy /= len;
    }
    if (dy > -0.25f) dy = -0.25f;
    const float spd = 3.2f + level * 0.22f;
    ball_x = (float)cx;
    ball_y = (float)cy;
    ball_dx = dx * spd;
    ball_dy = dy * spd;
    ball_live = true;
    aiming = false;
    buzzer_play(SFX_SHOOT);
    prev_bx = (int)ball_x;
    prev_by = (int)ball_y;
}

static bool hit_block(int c, int r) {
    const int hp = grid[r][c];
    if (hp <= 0) return false;
    const int x = c * cell_w() + 2;
    const int y = r * cell_h() + 4;
    const int w = cell_w() - 4;
    const int h = cell_h() - 4;
    const float bx = ball_x;
    const float by = ball_y;
    if (bx + BALL_R < x || bx - BALL_R > x + w || by + BALL_R < y || by - BALL_R > y + h)
        return false;

    grid[r][c] = (int8_t)(hp - 1);
    score += 10;
    buzzer_play(SFX_HIT);
    if (grid[r][c] <= 0)
        game_play_fill_rect(x - 1, y - 1, w + 2, h + 2, COL_BG);
    else
        draw_block(c, r);

    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    if (fabsf(bx - cx) > fabsf(by - cy))
        ball_dx = -ball_dx;
    else
        ball_dy = -ball_dy;
    return true;
}

static void physics_step() {
    ball_x += ball_dx;
    ball_y += ball_dy;

    if (ball_x < BALL_R) { ball_x = BALL_R; ball_dx = fabsf(ball_dx); }
    if (ball_x > PLAY_W - BALL_R) { ball_x = PLAY_W - BALL_R; ball_dx = -fabsf(ball_dx); }
    if (ball_y < BALL_R) { ball_y = BALL_R; ball_dy = fabsf(ball_dy); }

    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (hit_block(c, r)) return;

    if (ball_y >= LAUNCH_Y - 4 && ball_dy > 0) {
        ball_live = false;
        if (any_blocks_left()) {
            shift_blocks_down();
            if (blocks_in_danger()) {
                lives--;
                buzzer_play(SFX_ERROR);
                memset(grid, 0, sizeof(grid));
                for (int i = 0; i < 3 + level / 2; i++)
                    spawn_row(i);
            }
        } else {
            level++;
            score += 100 * level;
            buzzer_play(SFX_LEVEL);
            memset(grid, 0, sizeof(grid));
            for (int i = 0; i < 3 + level / 2; i++)
                spawn_row(i);
        }
    }
}

static void sync_ball_draw() {
    const int bx = (int)ball_x;
    const int by = (int)ball_y;
    if (bx == prev_bx && by == prev_by) return;
    game_play_fill_circle(prev_bx, prev_by, BALL_R + 1, COL_BG);
    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (grid[r][c] > 0) {
                const int x = c * cell_w() + 2;
                const int y = r * cell_h() + 4;
                if (bx + BALL_R >= x && bx - BALL_R <= x + cell_w() &&
                    by + BALL_R >= y && by - BALL_R <= y + cell_h() + 4)
                    draw_block(c, r);
            }
    game_play_fill_circle(bx, by, BALL_R, COL_BALL);
    prev_bx = bx;
    prev_by = by;
}

static void ballz_init(GameHud* hud) {
    score = 0;
    level = 1;
    lives = GAME_LIVES_DEFAULT;
    ball_live = false;
    aiming = false;
    last_phys = millis();
    memset(grid, 0, sizeof(grid));
    for (int i = 0; i < 4; i++)
        spawn_row(i);
    ballz_redraw();
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    game_hud_set_tier(hud, level);
}

void game_ballz_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_NIVEL, false);

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        hud->score_lower_better = false;
        ballz_init(hud);
        game_hud_set_score(hud, 0);

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
                ballz_redraw();

            if (!ball_live) {
                if (in.just_pressed && in.y >= PLAY_Y) {
                    aiming = true;
                    aim_px = in.play_x;
                    aim_py = in.play_y;
                }
                if (aiming && in.down) {
                    aim_px = in.play_x;
                    aim_py = in.play_y;
                    ballz_redraw();
                }
                if (aiming && in.just_released) {
                    aim_px = in.play_x;
                    aim_py = in.play_y;
                    launch_ball();
                }
            }

            if (ball_live && millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                physics_step();
                if (lives <= 0) {
                    dead = true;
                    break;
                }
                if (lives != hud->lives)
                    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
                if (level != hud->tier) {
                    if (game_hud_advance_tier(hud, level))
                        ballz_redraw();
                    else
                        game_hud_set_tier(hud, level);
                }
            }

            if (ball_live) {
                game_frame_draw_now();
                sync_ball_draw();
            }

            if (score != hud->score)
                game_hud_set_score(hud, score);
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
