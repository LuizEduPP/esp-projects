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

#define COLS       8
#define MAX_ROWS   8
#define MAX_BALLS  24
#define BALL_R     6
#define START_BALLS 3
#define LAUNCH_Y   (PLAY_H - 22)
#define DANGER_R   (MAX_ROWS - 1)
#define LAUNCH_GAP 52
#define COL_BG     0x0000
#define COL_BALL   0xFFFF
#define COL_BONUS  0xFFE0
#define COL_AIM    0x4208
#define PHYS_MS    16

typedef struct {
    float x, y, dx, dy;
    bool live;
    int prev_x, prev_y;
} Ball;

static int8_t grid[MAX_ROWS][COLS];
static bool bonus[MAX_ROWS][COLS];
static Ball balls[MAX_BALLS];
static int ball_stock;
static int launch_left;
static float launch_dx, launch_dy;
static uint32_t next_launch_ms;
static bool volley_active;
static bool aiming;
static int aim_px, aim_py;
static int score, level, lives;
static int volley_combo;
static uint32_t last_phys;

static int cell_w() { return PLAY_W / COLS; }
static int cell_h() { return 24; }

static uint16_t block_color(int hp) {
    static const uint16_t cols[] = {0x001F, 0x07FF, 0x07E0, 0xFFE0, 0xFD20, 0xF800, 0xF81F, 0xFFFF};
    if (hp <= 0) return COL_BG;
    if (hp > 8) return cols[7];
    return cols[hp - 1];
}

static int live_ball_count() {
    int n = 0;
    for (int i = 0; i < MAX_BALLS; i++)
        if (balls[i].live) n++;
    return n;
}

static void spawn_row(int row) {
    for (int c = 0; c < COLS; c++) {
        if (random(0, 100) < 32) {
            grid[row][c] = 0;
            bonus[row][c] = false;
            continue;
        }
        const int base = 1 + (level - 1) / 2;
        int hp = base + random(0, 2);
        if (hp > 9) hp = 9;
        grid[row][c] = (int8_t)hp;
        const int bonus_chance = level <= 2 ? 10 : (level <= 5 ? 14 : 18);
        bonus[row][c] = (random(0, 100) < bonus_chance);
    }
}

static void shift_blocks_down() {
    for (int r = MAX_ROWS - 1; r >= 1; r--) {
        memcpy(grid[r], grid[r - 1], COLS);
        memcpy(bonus[r], bonus[r - 1], COLS);
    }
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

static void draw_danger_line() {
    const int y = DANGER_R * cell_h() + 2;
    for (int x = 4; x < PLAY_W - 4; x += 10)
        game_play_fill_rect(x, y, 6, 2, 0xF800);
}

static bool overlaps_danger_line(int py, int r) {
    const int ly = DANGER_R * cell_h() + 2;
    return py + r >= ly && py - r <= ly + 2;
}

static void clear_danger_rows() {
    for (int r = DANGER_R - 1; r < MAX_ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            grid[r][c] = 0;
            bonus[r][c] = false;
        }
    }
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
    if (bonus[r][c]) {
        game_play_fill_circle(x + w - 6, y + 6, 4, COL_BONUS);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(0x0000, COL_BONUS);
        tft.drawString("+", PLAY_X + x + w - 6, PLAY_Y + y + 6, 1);
    }
    char s[2] = {(char)('0' + hp), 0};
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(hp >= 5 ? 0x0000 : 0xFFFF, col);
    tft.drawString(s, PLAY_X + x + w / 2, PLAY_Y + y + h / 2, 2);
}

static void draw_stock() {
    const int cx = PLAY_W / 2;
    const int y = LAUNCH_Y + 14;
    const int row_w = PLAY_W - 24;
    const int max_show = 14;
    const int show = ball_stock > max_show ? max_show : ball_stock;
    const int spacing = show > 0 ? min(row_w / show, 14) : 14;
    const int total_w = show * spacing;
    const int start_x = cx - total_w / 2 + spacing / 2;

    game_play_fill_rect(0, LAUNCH_Y + 2, PLAY_W, PLAY_H - LAUNCH_Y - 2, COL_BG);
    game_play_fill_round_rect(cx - 42, y - 10, 84, 28, 6, ui_tint565(0x2949, 10));

    for (int i = 0; i < show; i++) {
        const int bx = start_x + i * spacing;
        game_play_fill_circle(bx, y, 5, ui_tint565(COL_BALL, -30));
        game_play_fill_circle(bx, y, 4, COL_BALL);
    }

    char buf[12];
    if (ball_stock > max_show)
        snprintf(buf, sizeof(buf), "x%d +%d", max_show, ball_stock - max_show);
    else
        snprintf(buf, sizeof(buf), "x%d", ball_stock);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(COL_BALL, ui_tint565(0x2949, 10));
    tft.drawString(buf, PLAY_X + cx, PLAY_Y + y + 16, 2);
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
    draw_danger_line();
    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < COLS; c++)
            draw_block(c, r);
    game_play_fill_circle(PLAY_W / 2, LAUNCH_Y, 9, ui_tint565(0xFFFF, -40));
    game_play_fill_circle(PLAY_W / 2, LAUNCH_Y, 6, COL_BALL);
    draw_stock();
    for (int i = 0; i < MAX_BALLS; i++)
        if (balls[i].live)
            game_play_fill_circle((int)balls[i].x, (int)balls[i].y, BALL_R, COL_BALL);
    draw_aim_line();
    for (int i = 0; i < MAX_BALLS; i++) {
        balls[i].prev_x = (int)balls[i].x;
        balls[i].prev_y = (int)balls[i].y;
    }
}

static void spawn_ball(float dx, float dy) {
    for (int i = 0; i < MAX_BALLS; i++) {
        if (balls[i].live) continue;
        const float spd = 2.8f + (level - 1) * 0.16f;
        balls[i].x = (float)(PLAY_W / 2);
        balls[i].y = (float)LAUNCH_Y;
        balls[i].dx = dx * spd;
        balls[i].dy = dy * spd;
        balls[i].live = true;
        balls[i].prev_x = (int)balls[i].x;
        balls[i].prev_y = (int)balls[i].y;
        return;
    }
}

static void begin_volley() {
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
    launch_dx = dx;
    launch_dy = dy;
    launch_left = ball_stock;
    next_launch_ms = millis();
    volley_active = true;
    volley_combo = 0;
    aiming = false;
    buzzer_play(SFX_SHOOT);
}

static bool hit_block(Ball* b, int c, int r) {
    const int hp = grid[r][c];
    if (hp <= 0) return false;
    const int x = c * cell_w() + 2;
    const int y = r * cell_h() + 4;
    const int w = cell_w() - 4;
    const int h = cell_h() - 4;
    if (b->x + BALL_R < x || b->x - BALL_R > x + w || b->y + BALL_R < y || b->y - BALL_R > y + h)
        return false;

    grid[r][c] = (int8_t)(hp - 1);
    score += 10 + volley_combo;
    volley_combo++;
    if (volley_combo > 0 && volley_combo % 6 == 0)
        buzzer_play(SFX_RECORD);
    else if (grid[r][c] <= 0)
        buzzer_play(SFX_HIT);
    if (grid[r][c] <= 0) {
        if (bonus[r][c]) {
            ball_stock++;
            bonus[r][c] = false;
            buzzer_play(SFX_SCORE);
            draw_stock();
        }
        game_play_fill_rect(x - 1, y - 1, w + 2, h + 2, COL_BG);
    } else {
        draw_block(c, r);
    }

    const float cx = x + w * 0.5f;
    const float cy = y + h * 0.5f;
    if (fabsf(b->x - cx) > fabsf(b->y - cy))
        b->dx = -b->dx;
    else
        b->dy = -b->dy;
    return true;
}

static void step_ball(Ball* b) {
    b->x += b->dx;
    b->y += b->dy;

    if (b->x < BALL_R) { b->x = BALL_R; b->dx = fabsf(b->dx); }
    if (b->x > PLAY_W - BALL_R) { b->x = PLAY_W - BALL_R; b->dx = -fabsf(b->dx); }
    if (b->y < BALL_R) { b->y = BALL_R; b->dy = fabsf(b->dy); }

    for (int r = 0; r < MAX_ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (hit_block(b, c, r)) return;

    if (b->y >= LAUNCH_Y - 2 && b->dy > 0)
        b->live = false;
}

static void end_turn() {
    volley_active = false;
    launch_left = 0;
    if (any_blocks_left()) {
        shift_blocks_down();
        if (blocks_in_danger()) {
            lives--;
            buzzer_play(SFX_ERROR);
            clear_danger_rows();
            if (ball_stock < START_BALLS)
                ball_stock = START_BALLS;
        }
    } else {
        level++;
        score += 80 * level;
        buzzer_play(SFX_LEVEL);
        memset(grid, 0, sizeof(grid));
        memset(bonus, 0, sizeof(bonus));
        for (int i = 0; i < 2 + level / 3; i++)
            spawn_row(i);
    }
    ballz_redraw();
}

static void physics_step() {
    if (launch_left > 0 && (int32_t)(millis() - next_launch_ms) >= 0) {
        spawn_ball(launch_dx, launch_dy);
        launch_left--;
        next_launch_ms = millis() + LAUNCH_GAP;
    }

    for (int i = 0; i < MAX_BALLS; i++)
        if (balls[i].live)
            step_ball(&balls[i]);

    if (volley_active && launch_left <= 0 && live_ball_count() == 0)
        end_turn();
}

static void sync_balls_draw() {
    for (int i = 0; i < MAX_BALLS; i++) {
        Ball* b = &balls[i];
        if (!b->live) continue;
        const int bx = (int)b->x;
        const int by = (int)b->y;
        if (bx == b->prev_x && by == b->prev_y) continue;
        game_play_fill_circle(b->prev_x, b->prev_y, BALL_R + 1, COL_BG);
        if (overlaps_danger_line(b->prev_y, BALL_R + 1))
            draw_danger_line();
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
        b->prev_x = bx;
        b->prev_y = by;
    }
}

static void ballz_init(GameHud* hud) {
    score = 0;
    level = 1;
    lives = GAME_LIVES_DEFAULT;
    ball_stock = START_BALLS;
    launch_left = 0;
    volley_active = false;
    aiming = false;
    last_phys = millis();
    memset(grid, 0, sizeof(grid));
    memset(bonus, 0, sizeof(bonus));
    memset(balls, 0, sizeof(balls));
    for (int i = 0; i < 3; i++)
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
    game_hud_set_score_tag(hud, "Pts");

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

            if (!volley_active) {
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
                    begin_volley();
                }
            }

            if (volley_active && millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                physics_step();
            }

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

            if (volley_active) {
                game_frame_draw_now();
                sync_balls_draw();
            }

            if (score != hud->score)
                game_hud_set_score(hud, score);
            game_frame_delay();
        }

        const bool good_run = level >= 3 || score >= 300;
        if (game_hud_end_game(hud, score, good_run) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
