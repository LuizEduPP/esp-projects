#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "circus_sprites.h"
#include "buzzer.h"
#include "hw_config.h"
#include "display.h"
#include <Arduino.h>
#include <math.h>
#include <pgmspace.h>

#define GROUND_Y   (PLAY_H - 34)
#define PLAYER_W   33
#define PLAYER_H   31
#define JAR_W      25
#define JAR_H      25
#define RING_W     8
#define RING_GAP   20
#define RING_H     44
#define RING_TOP   (GROUND_Y - RING_H)
#define OBS_MAX    8
#define LIVES_MAX  GAME_LIVES_DEFAULT
#define PHYS_MS    16
#define STAGE_BASE_DIST 1600.0f
#define BLIT_MAX   (66 * 44)

#define COL_BG     0x1084
#define COL_TENT   0x6010

enum { OBS_JAR = 1, OBS_RING = 2 };

typedef struct {
    bool live;
    uint8_t kind;
    float wx;
    bool item;
    bool scored;
} Obstacle;

static float scroll_x;
static float prev_scroll_x;
static float player_x;
static float jump_y;
static bool jumping;
static uint32_t jump_start;
static int anim_frame;
static uint32_t last_anim;
static int prev_px, prev_py;
static int prev_anim_frame;
static int prev_fire_frame;
static int obs_prev_sx[OBS_MAX];

static Obstacle obs[OBS_MAX];
static int score, bonus, lives, stage;
static float stage_goal, next_spawn;
static uint32_t last_phys, last_bonus_tick;
static bool stage_clear_wait;
static uint32_t stage_clear_until;

static uint16_t blit_buf[BLIT_MAX];

static const CircusSprite* const SP_PLAYER[] = {
    &circus_sp_player0, &circus_sp_player1, &circus_sp_player2,
};
static const CircusSprite* const SP_JAR[] = { &circus_sp_jar0, &circus_sp_jar1 };
static const CircusSprite* const SP_RING_L[] = { &circus_sp_ring_l0, &circus_sp_ring_l1 };
static const CircusSprite* const SP_RING_R[] = { &circus_sp_ring_r0, &circus_sp_ring_r1 };

static float scroll_speed() {
    float s = 2.0f + (stage - 1) * 0.35f;
    return s > 4.2f ? 4.2f : s;
}

static int player_screen_y() {
    return (int)(GROUND_Y - PLAYER_H - jump_y);
}

static void circus_blit(int x, int y, const CircusSprite* sp) {
    if (!sp || x >= PLAY_W || y >= PLAY_H || x + sp->w <= 0 || y + sp->h <= 0) return;
    const int n = sp->w * sp->h;
    if (n > BLIT_MAX) return;
    for (int i = 0; i < n; i++)
        blit_buf[i] = pgm_read_word(&sp->data[i]);
    tft.pushImage(PLAY_X + x, PLAY_Y + y, sp->w, sp->h, blit_buf, CIRCUS_CHROMA);
}

static void draw_ground_band() {
    game_play_fill_rect(0, GROUND_Y, PLAY_W, PLAY_H - GROUND_Y, COL_TENT);
}

static void draw_bg_tiles() {
    const CircusSprite* bg = &circus_sp_bg_tile;
    const int off = (int)scroll_x % bg->w;
    for (int y = 0; y < GROUND_Y; y += bg->h) {
        for (int x = -off; x < PLAY_W; x += bg->w)
            circus_blit(x, y, bg);
    }
}

static void repair_bg_rect(int rx, int ry, int rw, int rh) {
    if (rw <= 0 || rh <= 0) return;
    const CircusSprite* bg = &circus_sp_bg_tile;
    if (ry < 0) ry = 0;
    if (ry + rh > GROUND_Y) rh = GROUND_Y - ry;
    for (int y = ry; y < ry + rh; y += bg->h) {
        int ts = ((int)scroll_x + rx) / bg->w * bg->w;
        for (; ts < (int)scroll_x + rx + rw + bg->w; ts += bg->w) {
            const int sx = ts - (int)scroll_x;
            if (sx + bg->w > rx && sx < rx + rw)
                circus_blit(sx, y, bg);
        }
    }
}

static void scroll_bg_strip(int ds) {
    if (ds <= 0) return;
    const CircusSprite* bg = &circus_sp_bg_tile;
    if (ds >= PLAY_W) {
        draw_bg_tiles();
        return;
    }
    const int x0 = PLAY_W - ds;
    for (int y = 0; y < GROUND_Y; y += bg->h) {
        int ts = ((int)scroll_x + x0) / bg->w * bg->w;
        for (; ts < (int)scroll_x + PLAY_W + bg->w; ts += bg->w) {
            const int sx = ts - (int)scroll_x;
            if (sx + bg->w > x0 && sx < PLAY_W)
                circus_blit(sx, y, bg);
        }
    }
    for (int y = 0; y < GROUND_Y; y += bg->h) {
        int ts = ((int)scroll_x) / bg->w * bg->w;
        for (; ts < (int)scroll_x + ds + bg->w; ts += bg->w) {
            const int sx = ts - (int)scroll_x;
            if (sx + bg->w > 0 && sx < ds)
                circus_blit(sx, y, bg);
        }
    }
}

static void obs_clear() {
    for (int i = 0; i < OBS_MAX; i++) {
        obs[i].live = false;
        obs_prev_sx[i] = -9999;
    }
}

static int obs_slot() {
    for (int i = 0; i < OBS_MAX; i++)
        if (!obs[i].live) return i;
    return -1;
}

static void spawn_obs() {
    const int slot = obs_slot();
    if (slot < 0) return;
    Obstacle* o = &obs[slot];
    o->live = true;
    o->scored = false;
    o->item = false;
    o->wx = scroll_x + PLAY_W + 40 + random(0, 30);
    o->kind = (random(0, 100) < (stage >= 2 ? 38 : 28)) ? OBS_RING : OBS_JAR;
    if (o->kind == OBS_RING && random(0, 100) < 22) o->item = true;
}

static void layout_stage() {
    scroll_x = 0;
    prev_scroll_x = -1.0f;
    player_x = PLAY_W * 0.22f;
    jump_y = 0;
    jumping = false;
    stage_goal = STAGE_BASE_DIST + (stage - 1) * 520.0f;
    next_spawn = scroll_x + PLAY_W + 60;
    obs_clear();
    spawn_obs();
    spawn_obs();
}

static void circus_init(GameHud* hud) {
    score = 0;
    bonus = 5000;
    lives = LIVES_MAX;
    stage = 1;
    stage_clear_wait = false;
    last_phys = millis();
    last_bonus_tick = millis();
    last_anim = millis();
    anim_frame = 0;
    prev_px = -1;
    prev_py = -1;
    prev_anim_frame = -1;
    prev_fire_frame = -1;
    layout_stage();
    game_hud_set_score_tag(hud, "Pts");
    game_hud_set_tier_mode(hud, HUD_TIER_FASE, false);
    game_hud_set_score(hud, 0);
    game_hud_set_tier(hud, stage);
    game_hud_set_lives(hud, lives, LIVES_MAX);
}

static int fire_frame() {
    return ((millis() - last_anim) / 180) & 1;
}

static void draw_jar(int sx) {
    if (sx < -JAR_W || sx > PLAY_W + JAR_W) return;
    circus_blit(sx, GROUND_Y - JAR_H, SP_JAR[fire_frame()]);
}

static void draw_ring(int sx, bool item) {
    if (sx > PLAY_W + RING_W * 2 + RING_GAP + 8) return;
    const int f = fire_frame();
    circus_blit(sx, RING_TOP, SP_RING_L[f]);
    circus_blit(sx + RING_W + RING_GAP, RING_TOP, SP_RING_R[f]);
    if (item) {
        const int cx = sx + RING_W + RING_GAP / 2 - circus_sp_cash.w / 2;
        circus_blit(cx, RING_TOP + 10, &circus_sp_cash);
    }
}

static void draw_player(int px, int py) {
    circus_blit(px, py, SP_PLAYER[anim_frame % 3]);
}

static void erase_player_sprite(int px, int py) {
    if (px < 0) return;
    repair_bg_rect(px - 2, py - 2, PLAYER_W + 4, PLAYER_H + 4);
}

static void sync_obstacles() {
    for (int i = 0; i < OBS_MAX; i++) {
        if (!obs[i].live) {
            obs_prev_sx[i] = -9999;
            continue;
        }
        const int sx = (int)(obs[i].wx - scroll_x);
        if (obs_prev_sx[i] != -9999 && obs_prev_sx[i] != sx) {
            if (obs[i].kind == OBS_JAR)
                repair_bg_rect(obs_prev_sx[i] - 2, GROUND_Y - JAR_H - 2, JAR_W + 4, JAR_H + 4);
            else
                repair_bg_rect(obs_prev_sx[i] - 2, RING_TOP - 2, RING_W * 2 + RING_GAP + 4, RING_H + 4);
        }
        if (obs[i].kind == OBS_JAR) draw_jar(sx);
        else draw_ring(sx, obs[i].item);
        obs_prev_sx[i] = sx;
    }
}

static void circus_redraw_full() {
    game_play_clear(COL_BG);
    draw_bg_tiles();
    draw_ground_band();
    for (int i = 0; i < OBS_MAX; i++) obs_prev_sx[i] = -9999;
    sync_obstacles();
    prev_px = (int)player_x;
    prev_py = player_screen_y();
    draw_player(prev_px, prev_py);
    prev_scroll_x = scroll_x;
    prev_anim_frame = anim_frame;
    prev_fire_frame = fire_frame();
}

static void sync_circus_draw() {
    const int ds = (int)(scroll_x - prev_scroll_x);
    if (prev_scroll_x < 0 || ds >= PLAY_W)
        draw_bg_tiles();
    else if (ds > 0)
        scroll_bg_strip(ds);

    sync_obstacles();

    const int px = (int)player_x;
    const int py = player_screen_y();
    const int pf = anim_frame % 3;
    if (px != prev_px || py != prev_py || pf != prev_anim_frame) {
        if (prev_px >= 0)
            erase_player_sprite(prev_px, prev_py);
        draw_player(px, py);
        prev_px = px;
        prev_py = py;
    }

    prev_scroll_x = scroll_x;
    prev_anim_frame = pf;
    prev_fire_frame = fire_frame();
}

static void player_hitbox(int* ox, int* oy, int* ow, int* oh) {
    *ox = (int)player_x + 8;
    *oy = player_screen_y() + 8;
    *ow = PLAYER_W - 14;
    *oh = PLAYER_H - 10;
}

static bool rects_hit(int ax, int ay, int aw, int ah, int bx, int by, int bw, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static void try_score_pass(Obstacle* o, int sx) {
    if (o->scored || sx + JAR_W > (int)player_x) return;
    if (o->kind == OBS_JAR && jump_y > 16.0f) {
        o->scored = true;
        score += 200;
        buzzer_play(SFX_TICK);
    } else if (o->kind == OBS_RING && jump_y > 18.0f) {
        o->scored = true;
        score += 100;
        if (o->item) {
            score += 1000;
            buzzer_play(SFX_RECORD);
        } else {
            buzzer_play(SFX_TICK);
        }
    }
}

static bool collide_obs(const Obstacle* o, int sx) {
    int px, py, pw, ph;
    player_hitbox(&px, &py, &pw, &ph);

    if (o->kind == OBS_JAR) {
        if (jump_y > 20.0f) return false;
        return rects_hit(px, py, pw, ph, sx + 4, GROUND_Y - JAR_H + 4, JAR_W - 8, JAR_H - 6);
    }

    const int gx0 = sx + RING_W;
    const int gx1 = sx + RING_W + RING_GAP;
    if (jump_y > 12.0f && jump_y < 42.0f && px + pw > gx0 + 1 && px < gx1 - 1)
        return false;
    if (rects_hit(px, py, pw, ph, sx, RING_TOP + 4, RING_W, RING_H - 8)) return true;
    if (rects_hit(px, py, pw, ph, gx1, RING_TOP + 4, RING_W, RING_H - 8)) return true;
    return false;
}

static void step_bonus() {
    if (stage_clear_wait) return;
    if (millis() - last_bonus_tick < 300) return;
    last_bonus_tick = millis();
    if (bonus > 0) bonus -= 10;
}

static void player_respawn() {
    jump_y = 0;
    jumping = false;
    player_x = PLAY_W * 0.22f;
    scroll_x -= 100.0f;
    if (scroll_x < 0) scroll_x = 0;
    prev_scroll_x = -1.0f;
}

static void step_jump() {
    if (!jumping) return;
    const float t = (float)(millis() - jump_start) / 520.0f;
    if (t >= 1.0f) {
        jumping = false;
        jump_y = 0;
        return;
    }
    jump_y = sinf(t * 3.14159265f) * 46.0f;
}

static void step_physics(GameHud* hud, const GameInput* in) {
    scroll_x += scroll_speed();

    if (in->down && in->y >= PLAY_Y) {
        if (in->play_x < PLAY_W / 3) player_x -= 2.4f;
        else if (in->play_x > PLAY_W * 2 / 3) player_x += 2.4f;
    }
    player_x = constrain(player_x, 10.0f, (float)(PLAY_W - PLAYER_W - 6));

    if (in->just_pressed && in->y >= PLAY_Y && !jumping) {
        jumping = true;
        jump_start = millis();
        buzzer_play(SFX_SHOOT);
    }

    step_jump();

    if (millis() - last_anim > 110) {
        last_anim = millis();
        anim_frame++;
    }

    while (next_spawn < scroll_x + PLAY_W + 80) {
        spawn_obs();
        next_spawn += 90 + random(0, (int)(70 + stage * 10));
    }

    for (int i = 0; i < OBS_MAX; i++) {
        if (!obs[i].live) continue;
        const int sx = (int)(obs[i].wx - scroll_x);
        try_score_pass(&obs[i], sx);
        if (sx < -70) {
            obs[i].live = false;
            continue;
        }
        if (collide_obs(&obs[i], sx)) {
            lives--;
            buzzer_play(SFX_ERROR);
            game_hud_set_lives(hud, lives, LIVES_MAX);
            player_respawn();
            if (lives <= 0) return;
            game_hud_show_toast(hud, "Queda!");
            circus_redraw_full();
            return;
        }
    }

    if (scroll_x >= stage_goal) {
        stage_clear_wait = true;
        stage_clear_until = millis() + 1200;
        score += bonus;
        bonus = 5000;
        buzzer_play(SFX_LEVEL);
        game_hud_show_toast(hud, "Palco!");
    }

    step_bonus();
    sync_circus_draw();
}

void game_circus_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        circus_init(hud);
        circus_redraw_full();

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
                circus_redraw_full();

            if (stage_clear_wait) {
                if (millis() >= stage_clear_until) {
                    stage_clear_wait = false;
                    stage++;
                    game_hud_advance_tier(hud, stage);
                    layout_stage();
                    circus_redraw_full();
                }
                game_frame_delay();
                continue;
            }

            if (millis() - last_phys >= PHYS_MS) {
                last_phys = millis();
                step_physics(hud, &in);
                if (lives <= 0) {
                    dead = true;
                    break;
                }
            }

            if (score != hud->score)
                game_hud_set_score(hud, score);
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
