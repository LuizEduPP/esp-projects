#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "ui_theme.h"
#include "buzzer.h"
#include "hw_config.h"
#include <Arduino.h>

#define BTN_GAP    6
#define FLASH_ON   480
#define FLASH_GAP  260
#define ROUND_WAIT 1200
#define COL_BG     0x1084

static const uint16_t COL_OFF[4] = {0x6000, 0x0300, 0x6200, 0x0008};
static const uint16_t COL_ON[4]  = {0xF800, 0x07E0, 0xFFE0, 0x001F};

static uint8_t seq[32];
static int seq_len;
static int step_idx;
static int input_idx;
static int score;
static int lives;
static bool playing_seq;
static bool waiting_input;
static uint32_t flash_until;
static int flash_btn;
static int flash_phase;
static int s_flash_on = FLASH_ON;

enum { PH_WAIT = 0, PH_LIT, PH_GAP };

static void btn_rect(int i, int* x, int* y, int* w, int* h) {
    const int pw = (PLAY_W - BTN_GAP * 3) / 2;
    const int ph = (PLAY_H - BTN_GAP * 3) / 2;
    const int ox = BTN_GAP;
    const int oy = BTN_GAP;
    *w = pw;
    *h = ph;
    if (i == 0) { *x = ox; *y = oy; }
    else if (i == 1) { *x = ox + pw + BTN_GAP; *y = oy; }
    else if (i == 2) { *x = ox; *y = oy + ph + BTN_GAP; }
    else { *x = ox + pw + BTN_GAP; *y = oy + ph + BTN_GAP; }
}

static void draw_btn(int i, bool lit) {
    int x, y, w, h;
    btn_rect(i, &x, &y, &w, &h);
    game_play_fill_round_rect(x, y, w, h, 8, lit ? COL_ON[i] : COL_OFF[i]);
}

static void simon_redraw() {
    game_play_clear(COL_BG);
    for (int i = 0; i < 4; i++) draw_btn(i, false);
}

static int hit_btn(int16_t tx, int16_t ty) {
    for (int i = 0; i < 4; i++) {
        int x, y, w, h;
        btn_rect(i, &x, &y, &w, &h);
        if (tx >= x && tx < x + w && ty >= y && ty < y + h) return i;
    }
    return -1;
}

static void begin_sequence() {
    step_idx = 0;
    input_idx = 0;
    playing_seq = true;
    waiting_input = false;
    flash_btn = -1;
    flash_phase = PH_WAIT;
    flash_until = millis() + ROUND_WAIT;
    simon_redraw();
}

static void start_round() {
    if (seq_len < (int)(sizeof(seq) - 1))
        seq[seq_len++] = (uint8_t)random(0, 4);
    begin_sequence();
}

static void simon_restart_round() {
    seq_len = 0;
    playing_seq = false;
    waiting_input = false;
    flash_btn = -1;
    simon_redraw();
    start_round();
}

static void simon_init(GameHud* hud) {
    seq_len = 0;
    score = 0;
    lives = GAME_LIVES_DEFAULT;
    playing_seq = false;
    waiting_input = false;
    flash_btn = -1;
    simon_redraw();
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    game_play_hint("Preste atencao...", ui_theme_get()->accent, COL_BG);
    delay(ROUND_WAIT);
    game_play_clear_hint(COL_BG);
    start_round();
}

static void advance_sequence() {
    if (millis() < flash_until) return;

    if (flash_phase == PH_WAIT) {
        flash_phase = PH_LIT;
        flash_btn = seq[step_idx];
        game_frame_draw_now();
        draw_btn(flash_btn, true);
        buzzer_simon_tone(flash_btn);
        flash_until = millis() + (uint32_t)s_flash_on;
        return;
    }

    if (flash_phase == PH_LIT) {
        game_frame_draw_now();
        draw_btn(flash_btn, false);
        step_idx++;
        if (step_idx >= seq_len) {
            playing_seq = false;
            waiting_input = true;
            input_idx = 0;
            flash_btn = -1;
            return;
        }
        flash_phase = PH_GAP;
        flash_until = millis() + FLASH_GAP;
        return;
    }

    if (flash_phase == PH_GAP) {
        flash_phase = PH_LIT;
        flash_btn = seq[step_idx];
        game_frame_draw_now();
        draw_btn(flash_btn, true);
        buzzer_simon_tone(flash_btn);
        flash_until = millis() + (uint32_t)s_flash_on;
    }
}

void game_simon_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_FASE, false);

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        s_flash_on = cfg->speed > 0 ? (int)cfg->speed : FLASH_ON;
        simon_init(hud);
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
                simon_redraw();

            if (playing_seq)
                advance_sequence();

            if (waiting_input && in.just_pressed && in.y >= PLAY_Y) {
                const int b = hit_btn(in.play_x, in.play_y);
                if (b < 0) continue;
                game_frame_draw_now();
                draw_btn(b, true);
                buzzer_simon_tone(b);
                delay(220);
                game_frame_draw_now();
                draw_btn(b, false);
                if (b != seq[input_idx]) {
                    buzzer_play(SFX_ERROR);
                    lives--;
                    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
                    if (lives <= 0) {
                        dead = true;
                        break;
                    }
                    simon_restart_round();
                    continue;
                }
                input_idx++;
                if (input_idx >= seq_len) {
                    score = seq_len;
                    game_hud_set_score(hud, score);
                    if (game_hud_advance_tier(hud, seq_len))
                        simon_redraw();
                    delay(350);
                    start_round();
                }
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
