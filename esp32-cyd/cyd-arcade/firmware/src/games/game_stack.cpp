#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "hw_config.h"
#include <Arduino.h>

#define BLOCK_H   20
#define MAX_LVL   16
#define MOVE_MS   22
#define MIN_OVER  16

static const uint16_t COL_BG = 0x0000;
static const uint16_t COLS[] = {
    0xF800, 0xFD20, 0xFFE0, 0x07E0, 0x001F,
};

static int base_x[MAX_LVL];
static int base_w[MAX_LVL];
static int level, cam_base;
static int cur_x, cur_w, cur_dir, prev_cur_x;
static int score, last_hud_score;
static uint32_t last_step;

static int vis_level(int lv) { return lv - cam_base; }

static int block_y(int lv) {
    return PLAY_H - 28 - vis_level(lv) * BLOCK_H;
}

static void draw_block(int x, int y, int w, uint16_t col) {
    if (y < -BLOCK_H || y > PLAY_H) return;
    game_play_fill_round_rect(x, y, w, BLOCK_H - 2, 5, col);
}

static void erase_block(int x, int y, int w) {
    if (y < -BLOCK_H || y > PLAY_H) return;
    game_play_fill_rect(x - 1, y - 1, w + 2, BLOCK_H + 1, COL_BG);
}

static void draw_tower() {
    for (int i = cam_base; i < level; i++) {
        const int y = block_y(i);
        draw_block(base_x[i], y, base_w[i], COLS[i % 5]);
    }
}

static void draw_moving() {
    if (level >= MAX_LVL) return;
    draw_block(cur_x, block_y(level), cur_w, COLS[level % 5]);
}

static void stack_init() {
    level = 0;
    cam_base = 0;
    score = 0;
    last_hud_score = -1;
    base_w[0] = PLAY_W - 48;
    base_x[0] = 24;
    cur_w = base_w[0];
    cur_x = (PLAY_W - cur_w) / 2;
    cur_dir = 1;
    prev_cur_x = cur_x;
    last_step = millis();
    game_play_clear(COL_BG);
    draw_tower();
    draw_moving();
}

static void shift_camera() {
    for (int i = cam_base; i < level; i++)
        erase_block(base_x[i], block_y(i), base_w[i]);
    if (level < MAX_LVL)
        erase_block(cur_x, block_y(level), cur_w);

    cam_base++;

    draw_tower();
    draw_moving();
    prev_cur_x = cur_x;
}

static void stack_redraw() {
    game_play_clear(COL_BG);
    draw_tower();
    draw_moving();
}

static void scroll_cam_if_needed() {
    if (vis_level(level) < 10) return;
    shift_camera();
}

static void next_level() {
    level++;
    score += 10;
    scroll_cam_if_needed();
    if (level >= MAX_LVL) return;
    cur_w = base_w[level - 1];
    cur_x = cur_dir > 0 ? 8 : PLAY_W - cur_w - 8;
    cur_dir = -cur_dir;
    prev_cur_x = cur_x;
}

static bool place_block() {
    if (level == 0) {
        base_x[0] = cur_x;
        base_w[0] = cur_w;
        erase_block(cur_x, block_y(0), cur_w);
        draw_block(cur_x, block_y(0), cur_w, COLS[0]);
        next_level();
        return true;
    }

    const int px = base_x[level - 1];
    const int pw = base_w[level - 1];
    const int left = max(cur_x, px);
    const int right = min(cur_x + cur_w, px + pw);
    const int overlap = right - left;

    if (overlap < MIN_OVER) return false;

    erase_block(cur_x, block_y(level), cur_w);
    base_x[level] = left;
    base_w[level] = overlap;
    draw_block(left, block_y(level), overlap, COLS[level % 5]);

    if (overlap == cur_w && overlap == pw) score += 5;

    next_level();
    if (level < MAX_LVL)
        draw_moving();
    prev_cur_x = cur_x;
    return true;
}

void game_stack_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->title, cfg->engine, cfg->color);
    if (!hud) return;
    game_hud_set_level_prefix(hud, 'A');

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        stack_init();
        game_hud_set_score(hud, 0);
        game_hud_set_level(hud, 1);
        last_hud_score = 0;

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
                stack_redraw();

            if (millis() - last_step >= MOVE_MS) {
                last_step = millis();
                prev_cur_x = cur_x;
                cur_x += cur_dir * (2 + level / 3);
                if (cur_x <= 8) { cur_x = 8; cur_dir = 1; }
                if (cur_x + cur_w >= PLAY_W - 8) {
                    cur_x = PLAY_W - cur_w - 8;
                    cur_dir = -1;
                }
                if (level < MAX_LVL) {
                    erase_block(prev_cur_x, block_y(level), cur_w);
                    draw_moving();
                }
            }

            if (in.just_pressed && in.y >= PLAY_Y) {
                if (!place_block()) {
                    dead = true;
                    break;
                }
                if (score != last_hud_score) {
                    game_hud_set_score(hud, score);
                    last_hud_score = score;
                }
                game_hud_set_level(hud, level + 1);
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, level >= MAX_LVL - 1) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
