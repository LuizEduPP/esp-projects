#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TH ui_theme_get()

#define COLS       8
#define ROWS       5
#define PAD_W      64
#define PAD_H      12
#define PAD_WIDE   96
#define BALL_R     5
#define MAX_BALLS  2
#define LIVES_MAX  GAME_LIVES_DEFAULT
#define PHYS_MS    16
#define CAP_W      18
#define CAP_H      10
#define CAP_SPEED  1.6f

#define COL_BG     0x0000
#define COL_BALL   0xFFFF
#define COL_PAD    0xDEFB
#define COL_SILVER 0xC618
#define COL_GOLD   0xFFE0

static const uint16_t ROW_COLORS[ROWS] = {
    0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x001F,
};

enum {
    BRICK_EMPTY = 0,
    BRICK_COLOR,
    BRICK_SILVER,
    BRICK_GOLD,
};

enum {
    CAP_NONE = 0,
    CAP_EXPAND,
    CAP_SLOW,
    CAP_CATCH,
    CAP_MULTI,
};

typedef struct {
    float x, y, dx, dy;
    bool live;
    int prev_x, prev_y;
    uint8_t pad_cd;
} Ball;

static uint8_t brick_kind[ROWS][COLS];
static uint8_t brick_hp[ROWS][COLS];
static Ball balls[MAX_BALLS];
static int pad_x;
static bool ball_stuck;
static bool catch_armed;
static bool wide_pad;
static bool slow_ball;
static uint32_t wide_until, slow_until;
static int score, lives, level;
static bool level_cleared;
static bool hint_visible;
static uint32_t last_phys;

static int cap_type;
static float cap_x, cap_y;
static bool cap_active;

static int pad_w() {
    if (wide_pad && millis() < wide_until) return PAD_WIDE;
    if (wide_pad) wide_pad = false;
    return PAD_W;
}

static float ball_speed_cap() {
    float spd = 2.1f + level * 0.18f;
    if (slow_ball && millis() < slow_until) spd *= 0.55f;
    else if (slow_ball) slow_ball = false;
    return spd;
}

static int brick_w() { return PLAY_W / COLS; }
static int brick_h() { return 12 + (level > 3 ? 1 : 0); }
static int brick_top() { return PLAY_MARGIN + 8 + (level - 1) * 2; }
static int pad_y() { return PLAY_H - PLAY_MARGIN - PAD_H - 4; }
static int bricks_bottom_y() { return ROWS * brick_h() + brick_top() + 4; }

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
            if (brick_kind[r][c] != BRICK_EMPTY && brick_kind[r][c] != BRICK_GOLD)
                return true;
    return false;
}

static int live_ball_count() {
    int n = 0;
    for (int i = 0; i < MAX_BALLS; i++)
        if (balls[i].live) n++;
    return n;
}

static void reset_powers() {
    wide_pad = false;
    slow_ball = false;
    catch_armed = false;
    ball_stuck = false;
}

static void clear_bricks() {
    memset(brick_kind, 0, sizeof(brick_kind));
    memset(brick_hp, 0, sizeof(brick_hp));
}

static void set_brick(int r, int c, uint8_t kind, uint8_t hp) {
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS) return;
    brick_kind[r][c] = kind;
    brick_hp[r][c] = hp;
}

static void build_level_layout() {
    clear_bricks();
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            set_brick(r, c, BRICK_COLOR, 1);

    const int lv = ((level - 1) % 6) + 1;
    switch (lv) {
    case 1:
        break;
    case 2:
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++)
                if ((r + c) % 2 == 0) set_brick(r, c, BRICK_EMPTY, 0);
        break;
    case 3:
        for (int c = 0; c < COLS; c++) {
            set_brick(0, c, BRICK_SILVER, 2);
            set_brick(ROWS - 1, c, BRICK_GOLD, 1);
        }
        break;
    case 4:
        for (int r = 0; r < ROWS; r++)
            set_brick(r, COLS / 2, BRICK_EMPTY, 0);
        for (int c = 0; c < COLS; c++)
            if (c != 0 && c != COLS - 1) set_brick(0, c, BRICK_SILVER, 2);
        break;
    case 5:
        for (int c = 0; c < COLS; c++) {
            set_brick(0, c, BRICK_GOLD, 1);
            set_brick(1, c, BRICK_SILVER, 2);
        }
        break;
    default:
        for (int r = 0; r < ROWS; r++)
            for (int c = 0; c < COLS; c++) {
                if ((r + c + level) % 3 == 0) set_brick(r, c, BRICK_EMPTY, 0);
                else if (r == 0) set_brick(r, c, BRICK_SILVER, 2);
            }
        set_brick(0, 0, BRICK_GOLD, 1);
        set_brick(0, COLS - 1, BRICK_GOLD, 1);
        break;
    }

    if (!bricks_left()) {
        for (int c = 0; c < COLS; c++)
            set_brick(ROWS / 2, c, BRICK_COLOR, 1);
    }
}

static void init_level() {
    build_level_layout();
    pad_x = PLAY_W / 2;
    memset(balls, 0, sizeof(balls));
    balls[0].live = false;
    balls[1].live = false;
    ball_stuck = false;
    cap_active = false;
    cap_type = CAP_NONE;
    hint_visible = false;
    level_cleared = false;
    balls[0].x = (float)pad_x;
    balls[0].y = (float)(pad_y() - BALL_R - 4);
}

static void draw_brick(int r, int c) {
    const uint8_t kind = brick_kind[r][c];
    if (kind == BRICK_EMPTY) return;
    int x, y, w, h;
    brick_rect(r, c, &x, &y, &w, &h);
    uint16_t col = ROW_COLORS[r];
    if (kind == BRICK_SILVER) col = COL_SILVER;
    else if (kind == BRICK_GOLD) col = COL_GOLD;
    game_play_fill_round_rect(x, y, w, h, 2, col);
    if (kind == BRICK_SILVER && brick_hp[r][c] > 1) {
        game_play_fill_rect(x + 2, y + h / 2, w - 4, 2, 0xFFFF);
    }
}

static void redraw_bricks_in_rect(int rx, int ry, int rw, int rh) {
    if (ry >= bricks_bottom_y()) return;
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (brick_kind[r][c] == BRICK_EMPTY) continue;
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
    const int pw = pad_w();
    uint16_t col = COL_PAD;
    if (wide_pad) col = 0xFFE0;
    else if (catch_armed || ball_stuck) col = 0x07FF;
    game_play_fill_round_rect(px - pw / 2, pad_y(), pw, PAD_H, 5, col);
}

static void draw_ball_at(int bx, int by) {
    game_play_fill_circle(bx, by, BALL_R, COL_BALL);
}

static uint16_t cap_color(int type) {
    switch (type) {
    case CAP_EXPAND: return 0x07E0;
    case CAP_SLOW:   return 0x07FF;
    case CAP_CATCH:  return 0xF81F;
    case CAP_MULTI:  return 0xFD20;
    default:         return 0xFFFF;
    }
}

static const char* cap_label(int type) {
    switch (type) {
    case CAP_EXPAND: return "E";
    case CAP_SLOW:   return "S";
    case CAP_CATCH:  return "C";
    case CAP_MULTI:  return "D";
    default:         return "?";
    }
}

static void draw_capsule() {
    if (!cap_active) return;
    const int x = (int)cap_x - CAP_W / 2;
    const int y = (int)cap_y - CAP_H / 2;
    const uint16_t col = cap_color(cap_type);
    game_play_fill_round_rect(x, y, CAP_W, CAP_H, 4, col);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x0000, col);
    tft.drawString(cap_label(cap_type), PLAY_X + (int)cap_x, PLAY_Y + (int)cap_y, 1);
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
    const int pw = pad_w();
    erase_rect(px - pw / 2 - 1, pad_y() - 1, pw + 2, PAD_H + 2);
}

static void clear_launch_hint() {
    game_play_fill_rect(0, 2, PLAY_W, 28, COL_BG);
    redraw_bricks_in_rect(0, 2, PLAY_W, 28);
}

static bool waiting_to_launch() {
    return live_ball_count() == 0 && !ball_stuck;
}

static void draw_launch_hint() {
    const int hy = 6;
    const int bw = 156;
    const int bx = (PLAY_W - bw) / 2;
    game_play_fill_rect(0, 2, PLAY_W, 28, COL_BG);
    redraw_bricks_in_rect(0, 2, PLAY_W, 28);
    game_play_fill_round_rect(bx, hy, bw, 22, 5, TH->card);
    tft.drawRoundRect(PLAY_X + bx, PLAY_Y + hy, bw, 22, 5, TH->accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("TOQUE P/ LANCAR", PLAY_X + PLAY_W / 2, PLAY_Y + hy + 11, 1);
}

static void show_launch_hint_if_needed() {
    if (!waiting_to_launch()) {
        if (hint_visible) {
            clear_launch_hint();
            hint_visible = false;
        }
        return;
    }
    if (hint_visible) return;
    draw_launch_hint();
    hint_visible = true;
}

static void breakout_redraw(GameHud* hud) {
    game_play_clear(COL_BG);
    draw_all_bricks();
    draw_pad(pad_x);
    for (int i = 0; i < MAX_BALLS; i++)
        if (balls[i].live)
            draw_ball_at((int)balls[i].x, (int)balls[i].y);
    if (waiting_to_launch())
        draw_ball_at((int)balls[0].x, (int)balls[0].y);
    draw_capsule();
    hint_visible = false;
    show_launch_hint_if_needed();
    (void)hud;
}

static void spawn_capsule(int cx, int cy) {
    if (cap_active || random(0, 100) >= 28) return;
    static const int pool[] = { CAP_EXPAND, CAP_SLOW, CAP_CATCH, CAP_MULTI };
    cap_type = pool[random(0, 4)];
    cap_x = (float)cx;
    cap_y = (float)cy;
    cap_active = true;
}

static void apply_capsule(int type) {
    switch (type) {
    case CAP_EXPAND:
        wide_pad = true;
        wide_until = millis() + 14000;
        break;
    case CAP_SLOW:
        slow_ball = true;
        slow_until = millis() + 12000;
        break;
    case CAP_CATCH:
        catch_armed = true;
        break;
    case CAP_MULTI:
        for (int i = 1; i < MAX_BALLS; i++) {
            if (balls[i].live) continue;
            if (!balls[0].live) break;
            balls[i] = balls[0];
            balls[i].dx = -balls[0].dx;
            balls[i].dy = balls[0].dy;
            balls[i].live = true;
            balls[i].pad_cd = 0;
            balls[i].prev_x = (int)balls[i].x;
            balls[i].prev_y = (int)balls[i].y;
            break;
        }
        break;
    default:
        break;
    }
    buzzer_play(SFX_RECORD);
}

static void launch_ball() {
    balls[0].x = (float)pad_x;
    balls[0].y = (float)(pad_y() - BALL_R - 4);
    const float angle = random(-20, 21) * 0.01745f;
    const float spd = ball_speed_cap();
    balls[0].dx = sinf(angle) * spd;
    balls[0].dy = -cosf(angle) * spd;
    balls[0].live = true;
    balls[0].pad_cd = 0;
    balls[0].prev_x = (int)balls[0].x;
    balls[0].prev_y = (int)balls[0].y;
    ball_stuck = false;
    hint_visible = false;
    clear_launch_hint();
    buzzer_play(SFX_SHOOT);
}

static void stick_ball_to_pad() {
    ball_stuck = true;
    catch_armed = false;
    for (int i = 0; i < MAX_BALLS; i++) {
        if (!balls[i].live) continue;
        balls[i].x = (float)pad_x;
        balls[i].y = (float)(pad_y() - BALL_R - 4);
        balls[i].dx = 0;
        balls[i].dy = 0;
    }
}

static bool collide_brick(Ball* b) {
    const int bx = (int)b->x;
    const int by = (int)b->y;

    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            const uint8_t kind = brick_kind[r][c];
            if (kind == BRICK_EMPTY) continue;
            int rx, ry, rw, rh;
            brick_rect(r, c, &rx, &ry, &rw, &rh);
            if (bx + BALL_R <= rx || bx - BALL_R >= rx + rw ||
                by + BALL_R <= ry || by - BALL_R >= ry + rh)
                continue;

            if (kind == BRICK_GOLD) {
                const int cx = rx + rw / 2;
                if (bx < cx) b->dx = -fabsf(b->dx);
                else b->dx = fabsf(b->dx);
                if (by < ry + rh / 2) b->dy = -fabsf(b->dy);
                else b->dy = fabsf(b->dy);
                buzzer_play(SFX_TICK);
                return true;
            }

            if (brick_hp[r][c] > 1) {
                brick_hp[r][c]--;
                draw_brick(r, c);
                buzzer_play(SFX_TICK);
            } else {
                brick_kind[r][c] = BRICK_EMPTY;
                score += 10 + (ROWS - r) * 5 + (kind == BRICK_SILVER ? 20 : 0);
                spawn_capsule(rx + rw / 2, ry + rh / 2);
                game_play_fill_rect(rx - 1, ry - 1, rw + 2, rh + 2, COL_BG);
                buzzer_play(SFX_HIT);
            }

            const int cx = rx + rw / 2;
            if (bx < cx) b->dx = -fabsf(b->dx);
            else b->dx = fabsf(b->dx);
            if (by < ry + rh / 2) b->dy = -fabsf(b->dy);
            else b->dy = fabsf(b->dy);
            return true;
        }
    }
    return false;
}

static void normalize_ball(Ball* b) {
    const float cap = ball_speed_cap();
    const float len = sqrtf(b->dx * b->dx + b->dy * b->dy);
    if (len < 0.01f) return;
    b->dx = b->dx / len * cap;
    b->dy = b->dy / len * cap;
}

static bool collide_pad(Ball* b, float ox, float oy) {
    if (b->dy <= 0.0f) return false;

    const float py = (float)pad_y();
    const float pw = (float)pad_w();
    const float pl = (float)pad_x - pw * 0.5f - BALL_R;
    const float pr = (float)pad_x + pw * 0.5f + BALL_R;
    const float pb = py + PAD_H + BALL_R;

    const float prev_bot = oy + BALL_R;
    const float curr_bot = b->y + BALL_R;
    if (prev_bot < py - BALL_R && curr_bot < py - BALL_R) return false;

    const bool crossed = prev_bot <= pb && curr_bot >= py - 2.0f;
    const bool inside = curr_bot >= py && b->y - BALL_R <= pb;
    if (!crossed && !inside) return false;

    const float xlo = ox - BALL_R < b->x - BALL_R ? ox - BALL_R : b->x - BALL_R;
    const float xhi = ox + BALL_R > b->x + BALL_R ? ox + BALL_R : b->x + BALL_R;
    if (xhi < pl || xlo > pr) return false;

    b->y = py - BALL_R;
    if (catch_armed) {
        stick_ball_to_pad();
        return true;
    }

    const float min_up = 1.4f;
    b->dy = -fmaxf(fabsf(b->dy), min_up);
    b->pad_cd = 2;
    const float hit = (b->x - (float)pad_x) / (pw * 0.5f);
    b->dx = hit * 3.2f;
    if (fabsf(b->dx) < 0.6f) b->dx = b->dx < 0.0f ? -0.6f : 0.6f;
    normalize_ball(b);
    if (b->dy > -min_up) b->dy = -min_up;
    buzzer_play(SFX_TICK);
    return true;
}

static void ball_move_step(Ball* b, float sx, float sy) {
    const float ox = b->x;
    const float oy = b->y;
    b->x += sx;
    b->y += sy;

    if (b->x < BALL_R) { b->x = BALL_R; b->dx = fabsf(b->dx); }
    if (b->x > PLAY_W - BALL_R) { b->x = PLAY_W - BALL_R; b->dx = -fabsf(b->dx); }
    if (b->y < BALL_R) { b->y = BALL_R; b->dy = fabsf(b->dy); }

    if (b->pad_cd > 0) {
        b->pad_cd--;
        if (oy + BALL_R < (float)pad_y() - 8.0f) b->pad_cd = 0;
    }

    if (!collide_pad(b, ox, oy))
        collide_brick(b);
}

static void ball_physics(Ball* b) {
    if (!b->live) return;

    const float speed = sqrtf(b->dx * b->dx + b->dy * b->dy);
    int steps = 1;
    if (speed > 2.5f) steps = 2;
    if (speed > 4.0f) steps = 3;
    if (speed > 5.5f) steps = 5;
    if (speed > 7.0f) steps = 7;

    for (int i = 0; i < steps; i++) {
        const float sx = b->dx / (float)steps;
        const float sy = b->dy / (float)steps;
        ball_move_step(b, sx, sy);
        if (!b->live || ball_stuck) return;
        if (b->y > PLAY_H + BALL_R) {
            b->live = false;
            return;
        }
    }
}

static void step_capsule() {
    if (!cap_active) return;
    const int prev_y = (int)cap_y;
    cap_y += CAP_SPEED;

    game_play_fill_rect((int)cap_x - CAP_W / 2 - 1, prev_y - CAP_H / 2 - 1,
                        CAP_W + 2, CAP_H + 2, COL_BG);
    redraw_bricks_in_rect((int)cap_x - CAP_W / 2 - 1, prev_y - CAP_H / 2 - 1,
                          CAP_W + 2, CAP_H + 2);

    const int py = pad_y();
    const int pw = pad_w();
    if (cap_y + CAP_H / 2 >= py && cap_y - CAP_H / 2 <= py + PAD_H &&
        cap_x >= pad_x - pw / 2 && cap_x <= pad_x + pw / 2) {
        apply_capsule(cap_type);
        cap_active = false;
        return;
    }

    if (cap_y > PLAY_H + CAP_H) {
        cap_active = false;
        return;
    }
    draw_capsule();
}

static void physics_step() {
    level_cleared = false;
    const bool had_live = live_ball_count() > 0;

    if (ball_stuck) {
        for (int i = 0; i < MAX_BALLS; i++) {
            if (!balls[i].live) continue;
            balls[i].x = (float)pad_x;
            balls[i].y = (float)(pad_y() - BALL_R - 4);
        }
    } else {
        for (int i = 0; i < MAX_BALLS; i++)
            ball_physics(&balls[i]);
    }

    step_capsule();

    if (had_live && live_ball_count() == 0) {
        lives--;
        buzzer_play(SFX_ERROR);
        ball_stuck = false;
        catch_armed = false;
        hint_visible = false;
        clear_launch_hint();
    }

    if (live_ball_count() > 0 && !bricks_left()) {
        level++;
        score += 80 * level;
        init_level();
        level_cleared = true;
    }
}

static bool pad_near_any_ball() {
    const int py = pad_y();
    for (int i = 0; i < MAX_BALLS; i++) {
        if (!balls[i].live) continue;
        if (balls[i].y + BALL_R >= py - 2 && balls[i].y - BALL_R <= py + PAD_H + 2)
            return true;
    }
    return false;
}

static void sync_draw(int* prev_pad, int prev_px[], int prev_py[]) {
    if (*prev_pad != pad_x) {
        erase_pad_at(*prev_pad);
        draw_pad(pad_x);
        *prev_pad = pad_x;
    }

    for (int i = 0; i < MAX_BALLS; i++) {
        Ball* b = &balls[i];
        if (!b->live) {
            if (prev_px[i] >= 0) {
                erase_ball_at(prev_px[i], prev_py[i]);
                prev_px[i] = -1;
            }
            continue;
        }
        const int bx = (int)b->x;
        const int by = (int)b->y;
        if (bx != prev_px[i] || by != prev_py[i]) {
            if (prev_px[i] >= 0)
                erase_ball_at(prev_px[i], prev_py[i]);
            if (pad_near_any_ball())
                draw_pad(pad_x);
            draw_ball_at(bx, by);
            prev_px[i] = bx;
            prev_py[i] = by;
        }
    }

    if (waiting_to_launch()) {
        const int bx = (int)balls[0].x;
        const int by = (int)balls[0].y;
        if (prev_px[0] != bx || prev_py[0] != by) {
            if (prev_px[0] >= 0)
                erase_ball_at(prev_px[0], prev_py[0]);
            draw_ball_at(bx, by);
            prev_px[0] = bx;
            prev_py[0] = by;
        }
    }
}

static void breakout_init(GameHud* hud) {
    score = 0;
    lives = LIVES_MAX;
    level = 1;
    reset_powers();
    init_level();
    last_phys = millis();
    breakout_redraw(hud);
    game_hud_set_score(hud, 0);
    game_hud_set_tier(hud, level);
    game_hud_set_lives(hud, lives, LIVES_MAX);
}

void game_breakout_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_NIVEL, false);

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        breakout_init(hud);

        int prev_pad = pad_x;
        int prev_bx[MAX_BALLS];
        int prev_by[MAX_BALLS];
        for (int i = 0; i < MAX_BALLS; i++) {
            prev_bx[i] = -1;
            prev_by[i] = -1;
        }
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

            const bool any_live = live_ball_count() > 0;
            if (in.down && in.y >= PLAY_Y) {
                const int pw = pad_w();
                pad_x = constrain((int)in.play_x, pw / 2, PLAY_W - pw / 2);
                if ((!any_live || ball_stuck) && in.just_pressed)
                    launch_ball();
            }

            if (waiting_to_launch()) {
                balls[0].x = (float)pad_x;
                balls[0].y = (float)(pad_y() - BALL_R - 4);
            }

            if (millis() - last_phys >= PHYS_MS && !waiting_to_launch()) {
                last_phys = millis();
                physics_step();
                if (lives <= 0) {
                    dead = true;
                    break;
                }
                if (lives < prev_lives) {
                    game_hud_set_lives(hud, lives, LIVES_MAX);
                    prev_lives = lives;
                    for (int i = 0; i < MAX_BALLS; i++) {
                        prev_bx[i] = -1;
                        prev_by[i] = -1;
                    }
                }
                if (level_cleared) {
                    game_hud_advance_tier(hud, level);
                    breakout_redraw(hud);
                    prev_pad = pad_x;
                    for (int i = 0; i < MAX_BALLS; i++) {
                        prev_bx[i] = -1;
                        prev_by[i] = -1;
                    }
                }
            }

            sync_draw(&prev_pad, prev_bx, prev_by);

            show_launch_hint_if_needed();

            if (score != hud->score)
                game_hud_set_score(hud, score);
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
