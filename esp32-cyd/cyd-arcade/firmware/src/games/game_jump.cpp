#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "hw_config.h"
#include <Arduino.h>

#define PLAT_MAX  12
#define PLAT_H    12
#define PLAT_W    52
#define PLAYER_R  8
#define PHYS_MS   18

static const uint16_t COL_SKY    = 0x2D5F;
static const uint16_t COL_PLAYER = 0xFD20;
static const uint16_t COL_PLAT[6] = {
    0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x07FF, 0xF81F,
};

typedef struct { int x, y; bool on; } Plat;

static Plat plats[PLAT_MAX];
static float px, py, vy;
static int score, last_hud_score;
static uint32_t last_phys;
static int prev_ix, prev_iy;

static void gen_plat(int i, int y) {
    plats[i].x = random(12, PLAY_W - PLAT_W - 12);
    plats[i].y = y;
    plats[i].on = true;
}

static int top_plat_y() {
    int m = plats[0].y;
    for (int i = 1; i < PLAT_MAX; i++)
        if (plats[i].on && plats[i].y < m) m = plats[i].y;
    return m;
}

static uint16_t plat_color(int i) {
    return COL_PLAT[i % 6];
}

static void draw_plat(const Plat* p, int idx) {
    if (!p->on) return;
    game_play_fill_round_rect(p->x, p->y, PLAT_W, PLAT_H, 6, plat_color(idx));
}

static void erase_plat(const Plat* p) {
    if (!p->on) return;
    game_play_fill_round_rect(p->x, p->y, PLAT_W, PLAT_H, 6, COL_SKY);
}

static void draw_player(int x, int y) {
    game_play_fill_circle(x, y, PLAYER_R, COL_PLAYER);
}

static void erase_player(int x, int y) {
    game_play_fill_circle(x, y, PLAYER_R + 1, COL_SKY);
}

static void paint_all() {
    for (int i = 0; i < PLAT_MAX; i++) draw_plat(&plats[i], i);
    draw_player((int)px, (int)py);
    prev_ix = (int)px;
    prev_iy = (int)py;
}

static void jump_init() {
    px = PLAY_W / 2.0f;
    py = PLAY_H - 70.0f;
    vy = -8.5f;
    score = 0;
    last_hud_score = -1;
    for (int i = 0; i < PLAT_MAX; i++)
        gen_plat(i, PLAY_H - 24 - i * 44);
    plats[0].x = (int)px - PLAT_W / 2;
    plats[0].y = (int)py + PLAYER_R + 2;
    prev_ix = (int)px;
    prev_iy = (int)py;
    last_phys = millis();
    game_play_clear(COL_SKY);
    paint_all();
}

static void scroll_world(int dy) {
    if (dy <= 0) return;

    erase_player(prev_ix, prev_iy);
    for (int i = 0; i < PLAT_MAX; i++)
        erase_plat(&plats[i]);

    py += (float)dy;
    for (int i = 0; i < PLAT_MAX; i++) {
        if (!plats[i].on) continue;
        plats[i].y += dy;
        if (plats[i].y > PLAY_H + 10) {
            plats[i].y = top_plat_y() - random(40, 56);
            plats[i].x = random(12, PLAY_W - PLAT_W - 12);
        }
    }
    score += dy / 5;

    game_play_fill_rect(0, 0, PLAY_W, dy + 4, COL_SKY);
    paint_all();
}

static void physics_step(int target_x) {
    px += (target_x - px) * 0.18f;
    if (px < -PLAYER_R) px = PLAY_W + PLAYER_R;
    if (px > PLAY_W + PLAYER_R) px = -PLAYER_R;

    vy += 0.34f;
    py += vy;

    if (vy > 0) {
        const int pb = (int)py + PLAYER_R;
        for (int i = 0; i < PLAT_MAX; i++) {
            const Plat* p = &plats[i];
            if (!p->on) continue;
            if (pb >= p->y && pb <= p->y + PLAT_H + 3) {
                if ((int)px + PLAYER_R > p->x + 2 && (int)px - PLAYER_R < p->x + PLAT_W - 2) {
                    vy = -8.5f - min(score / 120, 4);
                    py = (float)(p->y - PLAYER_R);
                    break;
                }
            }
        }
    }

    const int cam = PLAY_H / 3;
    if (py < cam)
        scroll_world(cam - (int)py);
}

void game_jump_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->title, cfg->engine, cfg->color);
    if (!hud) return;

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        jump_init();
        game_hud_set_score(hud, 0);
        last_hud_score = 0;

        GameInput in;
        int target_x = (int)px;
        bool dead = false;

        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                paint_all();

            if (in.down && in.y >= PLAY_Y)
                target_x = in.play_x;

            if (millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                physics_step(target_x);
                if (py > PLAY_H + 24) dead = true;
            }

            const int ix = (int)px, iy = (int)py;
            if (ix != prev_ix || iy != prev_iy) {
                erase_player(prev_ix, prev_iy);
                for (int i = 0; i < PLAT_MAX; i++) {
                    const Plat* p = &plats[i];
                    if (!p->on) continue;
                    if (p->y <= prev_iy + PLAYER_R + 2 &&
                        p->y + PLAT_H >= prev_iy - PLAYER_R - 2)
                        draw_plat(p, i);
                }
                draw_player(ix, iy);
                prev_ix = ix;
                prev_iy = iy;
            }

            if (score != last_hud_score) {
                game_hud_set_score(hud, score);
                last_hud_score = score;
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
