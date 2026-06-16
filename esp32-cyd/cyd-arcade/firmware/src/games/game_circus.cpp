#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "circus_sprites.h"
#include "audio.h"
#include "hw_config.h"
#include "display.h"
#include <Arduino.h>
#include <math.h>
#include <pgmspace.h>

/* Escala WinAPI 512×448 → CYD (ver HyunjungLee-dev/Circus-Charlie) */
#define ORIG_H       448.0f
#define SCALE_F      (PLAY_H / ORIG_H)
#define SCROLL_LEN   (0.5f * SCALE_F)
#define JAR_GAP      (550.0f * SCALE_F)

#define AUDIENCE_Y   0
#define FIELD_Y      53
#define RING_BOTTOM  ((int)(325.0f * SCALE_F) + (int)(40.0f * SCALE_F))
#define JAR_Y        ((int)(360.0f * SCALE_F))
#define PLAYER_Y0    ((int)(345.0f * SCALE_F))
#define METER_Y      (PLAY_H - 22)
#define GROUND_Y     JAR_Y
#define PLAYER_X0    (100.0f * PLAY_W / 512.0f)
#define JUMP_AMP     (120.0f * SCALE_F)
#define JUMP_FWD     (38.0f * SCALE_F)
#define JUMP_MS      900
#define JUMP_CLEAR   (8.0f * SCALE_F)
#define RING_GAP     ((int)(12.0f * SCALE_F))
#define DECO_EVERY   7
#define DECO_AT      2
#define METER_EVERY  88
#define COL_SKY      0x0000
#define COL_FIELD    0x07E0
#define PLAYER_W     42
#define PLAYER_H     40
#define JAR_W        32
#define JAR_H        32
#define JAR_N        10
#define RING_N       4
#define LIVES_MAX    GAME_LIVES_DEFAULT
#define PHYS_MS      16
#define RING_SPAWN_MS 3500
#define RING_MIN_GAP   ((int)((JUMP_MS / (float)PHYS_MS) * 2.0f * SCROLL_LEN + 52.0f))
#define BLIT_MAX     (80 * 80)

enum { OBS_JAR = 1, OBS_RING = 2 };

typedef struct {
    bool live;
    float wx;
    bool item;
    bool scored;
} RingPair;

static float scroll_x;
static float prev_scroll_x;
static float player_base_x;
static float player_x;
static float jump_y;
static bool jumping;
static float jump_origin_x;
static uint32_t jump_start;
static int anim_frame;
static uint32_t last_anim;
static int prev_px, prev_py;
static int prev_anim_frame;
static int prev_fire_frame;
static int jar_prev_sx[JAR_N];
static int ring_prev_sx[RING_N];
static float jars_wx[JAR_N];
static bool jar_scored[JAR_N];
static RingPair rings[RING_N];
static int score, bonus, lives, stage;
static uint32_t last_phys, last_bonus_tick, last_ring_spawn;
static bool stage_clear_wait;
static uint32_t stage_clear_until;

static uint16_t blit_buf[BLIT_MAX];

static const CircusSprite* const SP_PLAYER[] = {
    &circus_sp_player0, &circus_sp_player1, &circus_sp_player2,
};
static const CircusSprite* const SP_JAR[] = { &circus_sp_jar0, &circus_sp_jar1 };
static const CircusSprite* const SP_RING_L[] = { &circus_sp_ring_l0, &circus_sp_ring_l1 };
static const CircusSprite* const SP_RING_R[] = { &circus_sp_ring_r0, &circus_sp_ring_r1 };

static int player_screen_y() {
    return (int)(PLAYER_Y0 - jump_y);
}

static int ring_pair_w() {
    return SP_RING_L[0]->w + RING_GAP + SP_RING_R[0]->w;
}

static float lane_left(int obs_w) {
    return player_base_x + (float)(PLAYER_W - obs_w) * 0.5f;
}

static float ring_spawn_wx() {
    return scroll_x + lane_left(ring_pair_w()) + (float)PLAY_W + 40.0f;
}

static int player_base_cx() {
    return (int)player_base_x + PLAYER_W / 2;
}

static int player_cx() {
    return (int)player_x + PLAYER_W / 2;
}

static void circus_blit(int x, int y, const CircusSprite* sp) {
    if (!sp || x >= PLAY_W || y >= PLAY_H || x + sp->w <= 0 || y + sp->h <= 0) return;
    const int n = sp->w * sp->h;
    if (n > BLIT_MAX) return;
    for (int i = 0; i < n; i++)
        blit_buf[i] = pgm_read_word(&sp->data[i]);
    tft.pushImage(PLAY_X + x, PLAY_Y + y, sp->w, sp->h, blit_buf, CIRCUS_CHROMA);
}

static void draw_field_fill() {
    const CircusSprite* field = &circus_sp_field_way;
    const int fb = FIELD_Y + field->h;
    if (fb >= METER_Y) return;
    game_play_fill_rect(0, fb, PLAY_W, METER_Y - fb, COL_FIELD);
    for (int i = 0; i < 4; i++) {
        const int ly = fb + 18 + i * 36;
        if (ly >= METER_Y - 2) break;
        game_play_fill_rect(0, ly, PLAY_W, 1, COL_SKY);
    }
}

static void repair_field_lines_in_rect(int rx, int ry, int rw, int rh) {
    const int fb = FIELD_Y + circus_sp_field_way.h;
    for (int i = 0; i < 4; i++) {
        const int ly = fb + 18 + i * 36;
        if (ly >= METER_Y - 2) break;
        if (ly >= ry && ly < ry + rh)
            game_play_fill_rect(rx, ly, rw, 1, COL_SKY);
    }
}

static void draw_field_strip() {
    const CircusSprite* field = &circus_sp_field_way;
    const int off = (int)scroll_x % field->w;
    for (int x = -off; x < PLAY_W; x += field->w)
        circus_blit(x, FIELD_Y, field);
    draw_field_fill();
}

static void draw_miters() {
    const CircusSprite* m = &circus_sp_miter;
    const int start = (int)(scroll_x / METER_EVERY) * METER_EVERY - (int)scroll_x;
    for (int wx = start; wx < PLAY_W + m->w; wx += METER_EVERY) {
        if (wx + m->w < 0) continue;
        circus_blit(wx, METER_Y, m);
    }
}

static void draw_audience_strip() {
    const CircusSprite* tile = &circus_sp_bg_tile;
    const int off = (int)scroll_x % tile->w;
    int idx = (int)scroll_x / tile->w;
    if (scroll_x < 0) idx--;
    for (int x = -off; x < PLAY_W; x += tile->w, idx++) {
        const CircusSprite* sp = tile;
        if (((idx % DECO_EVERY) + DECO_EVERY) % DECO_EVERY == DECO_AT)
            sp = &circus_sp_bg_deco;
        circus_blit(x, AUDIENCE_Y, sp);
    }
}

static void draw_bg_tiles() {
    game_play_fill_rect(0, 0, PLAY_W, PLAY_H, COL_SKY);
    draw_audience_strip();
    draw_field_strip();
    draw_miters();
}

static int bg_band_bottom() {
    return METER_Y;
}

static void repair_field_rect(int rx, int ry, int rw, int rh) {
    if (rw <= 0 || rh <= 0) return;
    if (ry < 0) { rh += ry; ry = 0; }
    if (ry + rh > bg_band_bottom()) rh = bg_band_bottom() - ry;
    if (rh <= 0) return;

    const CircusSprite* field = &circus_sp_field_way;
    const int field_b = FIELD_Y + field->h;
    const int aud_b = AUDIENCE_Y + circus_sp_bg_tile.h;

    if (ry + rh <= AUDIENCE_Y || ry >= bg_band_bottom()) {
        game_play_fill_rect(rx, ry, rw, rh, COL_SKY);
        return;
    }

    if (ry < AUDIENCE_Y)
        game_play_fill_rect(rx, ry, rw, AUDIENCE_Y - ry, COL_SKY);

    if (ry < aud_b && ry + rh > AUDIENCE_Y) {
        int ts = ((int)scroll_x + rx) / circus_sp_bg_tile.w * circus_sp_bg_tile.w;
        for (; ts < (int)scroll_x + rx + rw + circus_sp_bg_tile.w; ts += circus_sp_bg_tile.w) {
            const int sx = ts - (int)scroll_x;
            if (sx + circus_sp_bg_tile.w <= rx || sx >= rx + rw) continue;
            int tidx = ts / circus_sp_bg_tile.w;
            const CircusSprite* sp = &circus_sp_bg_tile;
            if (((tidx % DECO_EVERY) + DECO_EVERY) % DECO_EVERY == DECO_AT)
                sp = &circus_sp_bg_deco;
            circus_blit(sx, AUDIENCE_Y, sp);
        }
    }

    if (ry < field_b && ry + rh > FIELD_Y) {
        int ts = ((int)scroll_x + rx) / field->w * field->w;
        for (; ts < (int)scroll_x + rx + rw + field->w; ts += field->w) {
            const int sx = ts - (int)scroll_x;
            if (sx + field->w > rx && sx < rx + rw)
                circus_blit(sx, FIELD_Y, field);
        }
    }

    if (ry + rh > field_b && ry < METER_Y) {
        const int fy = field_b > ry ? field_b : ry;
        const int fh = (ry + rh > METER_Y ? METER_Y : ry + rh) - fy;
        if (fh > 0) {
            game_play_fill_rect(rx, fy, rw, fh, COL_FIELD);
            repair_field_lines_in_rect(rx, fy, rw, fh);
        }
    }

    if (ry + rh > METER_Y && ry < PLAY_H) {
        const int my = METER_Y > ry ? METER_Y : ry;
        game_play_fill_rect(rx, my, rw, (ry + rh > PLAY_H ? PLAY_H : ry + rh) - my, COL_SKY);
    }
}

static void repair_bg_rect(int rx, int ry, int rw, int rh) {
    repair_field_rect(rx, ry, rw, rh);
}

static void scroll_bg_strip(int ds) {
    (void)ds;
    draw_audience_strip();
    draw_field_strip();
    draw_miters();
}

static void layout_jars() {
    const float lane = lane_left(JAR_W);
    const float lead = (float)PLAY_W + 40.0f + JAR_GAP * 1.95f;
    for (int i = 0; i < JAR_N; i++) {
        jars_wx[i] = lane + lead + (float)i * JAR_GAP;
        jar_scored[i] = false;
    }
}

static void rings_clear() {
    for (int i = 0; i < RING_N; i++) {
        rings[i].live = false;
        ring_prev_sx[i] = -9999;
    }
}

static int ring_slot() {
    for (int i = 0; i < RING_N; i++)
        if (!rings[i].live) return i;
    return -1;
}

static bool ring_spawn_clear() {
    const float spawn_wx = ring_spawn_wx();
    for (int i = 0; i < RING_N; i++) {
        if (!rings[i].live) continue;
        if (spawn_wx - rings[i].wx < (float)RING_MIN_GAP)
            return false;
    }
    return true;
}

static int ring_top_y() {
    return RING_BOTTOM - SP_RING_L[0]->h;
}

static void try_spawn_ring() {
    if (millis() - last_ring_spawn < RING_SPAWN_MS) return;
    if (!ring_spawn_clear()) return;
    int live = 0;
    for (int i = 0; i < RING_N; i++)
        if (rings[i].live) live++;
    if (live >= 3) return;
    const int slot = ring_slot();
    if (slot < 0) return;
    last_ring_spawn = millis();
    rings[slot].live = true;
    rings[slot].scored = false;
    rings[slot].item = (random(0, 100) > 70);
    rings[slot].wx = ring_spawn_wx();
}

static void commit_jump_land() {
    player_base_x = PLAYER_X0;
    player_x = PLAYER_X0;
    jump_y = 0;
    jumping = false;
}

static void layout_stage() {
    scroll_x = 0;
    prev_scroll_x = -1.0f;
    player_base_x = PLAYER_X0;
    player_x = PLAYER_X0;
    jump_y = 0;
    jumping = false;
    layout_jars();
    rings_clear();
    last_ring_spawn = millis();
    for (int i = 0; i < JAR_N; i++) jar_prev_sx[i] = -9999;
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
    circus_blit(sx, JAR_Y, SP_JAR[fire_frame()]);
}

static void draw_ring_pair(int sx, bool item) {
    const CircusSprite* rl = SP_RING_L[fire_frame()];
    const CircusSprite* rr = SP_RING_R[fire_frame()];
    const int top = ring_top_y();
    const int rw = ring_pair_w();
    if (sx > PLAY_W + rw + 8) return;
    circus_blit(sx, top, rl);
    circus_blit(sx + rl->w + RING_GAP, top, rr);
    if (item) {
        const int cx = sx + rw / 2 - circus_sp_cash.w / 2;
        circus_blit(cx, top + rl->h / 2 - 8, &circus_sp_cash);
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
    for (int i = 0; i < JAR_N; i++) {
        const int sx = (int)(jars_wx[i] - scroll_x);
        if (jar_prev_sx[i] != -9999 && jar_prev_sx[i] != sx)
            repair_bg_rect(jar_prev_sx[i] - 2, JAR_Y - 2, JAR_W + 4, JAR_H + 4);
        draw_jar(sx);
        jar_prev_sx[i] = sx;
    }

    for (int i = 0; i < RING_N; i++) {
        if (!rings[i].live) {
            ring_prev_sx[i] = -9999;
            continue;
        }
        const int sx = (int)(rings[i].wx - scroll_x);
        if (ring_prev_sx[i] != -9999 && ring_prev_sx[i] != sx) {
            repair_bg_rect(ring_prev_sx[i] - 2, ring_top_y() - 2, ring_pair_w() + 4, SP_RING_L[0]->h + 4);
        }
        draw_ring_pair(sx, rings[i].item);
        ring_prev_sx[i] = sx;
    }
}

static void circus_redraw_full() {
    game_play_clear(COL_SKY);
    draw_bg_tiles();
    for (int i = 0; i < JAR_N; i++) jar_prev_sx[i] = -9999;
    for (int i = 0; i < RING_N; i++) ring_prev_sx[i] = -9999;
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
    if (prev_scroll_x < 0 || abs(ds) >= PLAY_W)
        draw_bg_tiles();
    else if (ds != 0)
        scroll_bg_strip(ds);
    else
        draw_field_strip();

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

static void try_score_jar(int i, int sx) {
    if (jar_scored[i] || !jumping) return;
    const float pcx = player_x + PLAYER_W * 0.5f;
    if (pcx > (float)sx && pcx < (float)(sx + JAR_W)) {
        jar_scored[i] = true;
        score += 200;
        audio_play(SFX_TICK);
    }
}

static void try_score_ring(RingPair* r, int sx) {
    if (r->scored || !jumping) return;
    const float pcx = player_x + PLAYER_W * 0.5f;
    const int rw = ring_pair_w();
    if (pcx > (float)sx && pcx <= (float)(sx + rw)) {
        r->scored = true;
        if (r->item) {
            score += 1100;
            audio_play(SFX_RECORD);
        } else {
            score += 100;
            audio_play(SFX_TICK);
        }
    }
}

static bool collide_jar(int sx) {
    const int jcx = sx + JAR_W / 2;
    if (abs(jcx - player_base_cx()) > 14) return false;
    if (jumping && jump_y >= JUMP_CLEAR) return false;
    return true;
}

static bool collide_ring(const RingPair* r, int sx) {
    (void)r;
    const int ring_cx = sx + ring_pair_w() / 2;
    if (abs(ring_cx - player_base_cx()) > 18) return false;
    if (jumping) return false;
    return true;
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
    player_base_x = PLAYER_X0;
    player_x = PLAYER_X0;
    scroll_x -= 80.0f * SCALE_F;
    if (scroll_x < 0) scroll_x = 0;
    prev_scroll_x = -1.0f;
}

static void step_jump() {
    if (!jumping) {
        player_x = player_base_x;
        return;
    }
    const float t = (float)(millis() - jump_start) / (float)JUMP_MS;
    if (t >= 1.0f) {
        commit_jump_land();
        return;
    }
    const float s = sinf(t * 3.14159265f);
    const float fwd = (1.0f - cosf(t * 3.14159265f)) * 0.5f;
    jump_y = s * JUMP_AMP;
    player_x = jump_origin_x + fwd * JUMP_FWD;
}

static void handle_touch(const GameInput* in) {
    if (in->just_pressed && in->y >= PLAY_Y && !jumping) {
        jump_origin_x = player_base_x;
        jumping = true;
        jump_start = millis();
        audio_play(SFX_SHOOT);
    }
}

static void step_world_scroll() {
    scroll_x += SCROLL_LEN;

    for (int i = 0; i < RING_N; i++) {
        if (!rings[i].live) continue;
        rings[i].wx -= SCROLL_LEN;
        if (rings[i].wx < scroll_x - 60.0f)
            rings[i].live = false;
    }
}

static void step_physics(GameHud* hud, const GameInput* in) {
    handle_touch(in);
    step_world_scroll();
    try_spawn_ring();

    step_jump();
    player_x = constrain(player_x, 10.0f, (float)(PLAY_W - PLAYER_W - 6));

    if (millis() - last_anim > 80) {
        last_anim = millis();
        if (jumping && jump_y > 8.0f)
            anim_frame = 2;
        else
            anim_frame++;
    }

    for (int i = 0; i < JAR_N; i++) {
        const int sx = (int)(jars_wx[i] - scroll_x);
        try_score_jar(i, sx);
        if (collide_jar(sx)) {
            lives--;
            audio_play(SFX_ERROR);
            game_hud_set_lives(hud, lives, LIVES_MAX);
            player_respawn();
            if (lives <= 0) return;
            game_hud_show_toast(hud, "Queda!");
            circus_redraw_full();
            return;
        }
    }

    for (int i = 0; i < RING_N; i++) {
        if (!rings[i].live) continue;
        const int sx = (int)(rings[i].wx - scroll_x);
        try_score_ring(&rings[i], sx);
        if (collide_ring(&rings[i], sx)) {
            lives--;
            audio_play(SFX_ERROR);
            game_hud_set_lives(hud, lives, LIVES_MAX);
            player_respawn();
            if (lives <= 0) return;
            game_hud_show_toast(hud, "Queda!");
            circus_redraw_full();
            return;
        }
    }

    if (scroll_x >= JAR_GAP * (float)(JAR_N + 1)) {
        stage_clear_wait = true;
        stage_clear_until = millis() + 1200;
        score += bonus;
        bonus = 5000;
        audio_play(SFX_LEVEL);
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
