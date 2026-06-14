#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "hw_config.h"
#include <Arduino.h>

#define LANES     3
#define CAR_W     30
#define CAR_H     18
#define OBS_W     32
#define OBS_H     20
#define OBS_MAX   5
#define CAR_Y_OFF 28

static const uint16_t COL_ROAD  = 0x3186;
static const uint16_t COL_EDGE  = 0x0320;
static const uint16_t COL_DASH  = 0xFFE0;
static const uint16_t COL_CAR   = 0xFFE0;
static const uint16_t COL_OBS_A = 0xF800;
static const uint16_t COL_OBS_B = 0x001F;

static int car_lane;
static int prev_lane;
static int obs_lane[OBS_MAX];
static int obs_y[OBS_MAX];
static bool obs_on[OBS_MAX];
static uint16_t obs_col[OBS_MAX];
static int score;
static int lives;
static uint32_t last_step, step_ms, last_spawn;

static int lane_w() { return PLAY_W / LANES; }
static int lane_cx(int lane) { return lane * lane_w() + lane_w() / 2; }
static int car_y() { return PLAY_H - CAR_Y_OFF; }

static void draw_road() {
    game_play_clear(COL_ROAD);
    game_play_fill_rect(0, 0, 6, PLAY_H, COL_EDGE);
    game_play_fill_rect(PLAY_W - 6, 0, 6, PLAY_H, COL_EDGE);
    for (int i = 1; i < LANES; i++) {
        const int lx = i * lane_w();
        for (int y = 0; y < PLAY_H; y += 16)
            game_play_fill_rect(lx - 1, y, 2, 8, COL_DASH);
    }
}

static void draw_car() {
    const int cx = lane_cx(car_lane);
    const int y = car_y();
    game_play_fill_round_rect(cx - CAR_W / 2, y, CAR_W, CAR_H, 4, COL_CAR);
    game_play_fill_rect(cx - CAR_W / 2 + 4, y + 4, CAR_W - 8, 6, 0xFC60);
}

static void draw_obs(int i) {
    if (!obs_on[i]) return;
    const int cx = lane_cx(obs_lane[i]);
    game_play_fill_round_rect(cx - OBS_W / 2, obs_y[i], OBS_W, OBS_H, 3, obs_col[i]);
    game_play_fill_rect(cx - OBS_W / 2 + 4, obs_y[i] + 4, OBS_W - 8, OBS_H - 10, 0x0000);
}

static void erase_obs(int i) {
    if (!obs_on[i]) return;
    const int cx = lane_cx(obs_lane[i]);
    game_play_fill_rect(cx - OBS_W / 2 - 1, obs_y[i] - 1, OBS_W + 2, OBS_H + 2, COL_ROAD);
}

static void spawn_obs() {
    for (int i = 0; i < OBS_MAX; i++) {
        if (obs_on[i]) continue;
        obs_on[i] = true;
        obs_lane[i] = random(0, LANES);
        obs_y[i] = -OBS_H;
        obs_col[i] = (random(0, 2) == 0) ? COL_OBS_A : COL_OBS_B;
        return;
    }
}

static bool rects_hit(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static bool step_game() {
    game_frame_draw_now();
    for (int i = 0; i < OBS_MAX; i++) {
        if (!obs_on[i]) continue;
        erase_obs(i);
        obs_y[i] += 5 + score / 40;
        if (obs_y[i] > PLAY_H) {
            obs_on[i] = false;
            score += 10;
            buzzer_play(SFX_TICK);
            continue;
        }
        draw_obs(i);
        const int cx = lane_cx(obs_lane[i]);
        if (obs_lane[i] == car_lane &&
            rects_hit(lane_cx(car_lane) - CAR_W / 2, car_y(), CAR_W, CAR_H,
                      cx - OBS_W / 2, obs_y[i], OBS_W, OBS_H))
            return false;
    }
    return true;
}

static void dodge_redraw() {
    draw_road();
    for (int i = 0; i < OBS_MAX; i++)
        if (obs_on[i]) draw_obs(i);
    draw_car();
}

static void dodge_reset_after_hit() {
    car_lane = 1;
    prev_lane = 1;
    for (int i = 0; i < OBS_MAX; i++) obs_on[i] = false;
    last_spawn = millis();
    dodge_redraw();
}

static void dodge_init(const GameEntry* cfg, GameHud* hud) {
    car_lane = 1;
    prev_lane = 1;
    score = 0;
    lives = GAME_LIVES_DEFAULT;
    for (int i = 0; i < OBS_MAX; i++) obs_on[i] = false;
    step_ms = cfg->speed > 0 ? cfg->speed : 80;
    last_step = millis();
    last_spawn = millis();
    draw_road();
    draw_car();
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
}

void game_dodge_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;

    bool retry = false;
    GameDrag drag = {};
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        dodge_init(cfg, hud);
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
                dodge_redraw();

            if (in.just_pressed && in.y >= PLAY_Y)
                game_drag_begin(&drag, &in);
            if (in.down)
                game_drag_update(&drag, &in);
            if (in.just_released && drag.active) {
                const int swipe = game_drag_swipe_h(&drag);
                if (swipe < 0 && car_lane > 0) car_lane--;
                else if (swipe > 0 && car_lane < LANES - 1) car_lane++;
                else if (in.play_x < PLAY_W / 3 && car_lane > 0) car_lane--;
                else if (in.play_x > PLAY_W * 2 / 3 && car_lane < LANES - 1) car_lane++;
                drag.active = false;
            }

            if (car_lane != prev_lane) {
                game_frame_draw_now();
                draw_road();
                for (int i = 0; i < OBS_MAX; i++) draw_obs(i);
                draw_car();
                prev_lane = car_lane;
            }

            const uint32_t spawn_ms = 900 - (score / 25) * 20;
            if (millis() - last_spawn > (spawn_ms < 400 ? 400 : spawn_ms)) {
                last_spawn = millis();
                spawn_obs();
            }

            if (millis() - last_step >= (uint32_t)step_ms) {
                last_step = millis();
                if (!step_game()) {
                    lives--;
                    buzzer_play(SFX_ERROR);
                    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
                    if (lives <= 0) {
                        dead = true;
                        break;
                    }
                    dodge_reset_after_hit();
                    continue;
                }
                game_hud_set_score(hud, score);
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
