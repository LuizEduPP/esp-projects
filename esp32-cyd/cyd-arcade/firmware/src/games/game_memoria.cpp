#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "audio.h"
#include "display.h"
#include "hw_config.h"
#include "ui_draw.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <string.h>

#define COLS 4
#define ROWS 4
#define PAIRS 8
#define CARD_GAP 6
#define COL_BG 0x0000
#define LOCK_MS 750

/* Fundo do cartão + símbolo em alto contraste (pares complementares) */
static const uint32_t PAIR_BG_RGB[PAIRS] = {
    0xFF3B30u, 0x34C759u, 0x007AFFu, 0xFF9500u,
    0xAF52DEu, 0x00C7BEu, 0xFFCC00u, 0xFF2D55u,
};
static const uint32_t PAIR_FG_RGB[PAIRS] = {
    0xFFFFFFu, 0x1A1A1Au, 0xFFE566u, 0x1E3A8Au,
    0xFFE066u, 0xFF2D55u, 0x1A1A1Au, 0x00D4FFu,
};

static uint8_t deck[COLS * ROWS];
static bool open[COLS * ROWS];
static bool solved[COLS * ROWS];
static int first_pick;
static int second_pick;
static int moves;
static int match_streak;
static bool input_lock;
static uint32_t lock_until;
static int card_w, card_h;

static uint16_t pair_bg(int id) {
    return ui_rgb565(PAIR_BG_RGB[id % PAIRS]);
}

static uint16_t pair_fg(int id) {
    return ui_rgb565(PAIR_FG_RGB[id % PAIRS]);
}

static int idx(int c, int r) { return r * COLS + c; }

static void card_rect(int c, int r, int* x, int* y) {
    *x = CARD_GAP + c * (card_w + CARD_GAP);
    *y = CARD_GAP + r * (card_h + CARD_GAP);
}

static void shuffle_deck() {
    for (int i = 0; i < PAIRS; i++) {
        deck[i * 2] = (uint8_t)i;
        deck[i * 2 + 1] = (uint8_t)i;
    }
    for (int i = COLS * ROWS - 1; i > 0; i--) {
        const int j = random(0, i + 1);
        const uint8_t t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
    }
}

static void draw_pair_symbol(int id, int cx, int cy, int sz, uint16_t fg, uint16_t bg) {
    const int s = sz / 2;
    switch (id % 8) {
    case 0:
        game_play_fill_circle(cx, cy, s, fg);
        break;
    case 1:
        game_play_fill_round_rect(cx - s, cy - s, s * 2, s * 2, 2, fg);
        break;
    case 2:
        game_play_fill_circle(cx, cy - s / 2, s - 1, fg);
        game_play_fill_circle(cx - s + 1, cy + s / 2, s - 1, fg);
        game_play_fill_circle(cx + s - 1, cy + s / 2, s - 1, fg);
        break;
    case 3:
        game_play_fill_rect(cx - s / 2, cy - s, s, s * 2, fg);
        game_play_fill_rect(cx - s, cy - s / 2, s * 2, s, fg);
        break;
    case 4: {
        const int r1 = s - 1;
        game_play_fill_circle(cx, cy, r1, fg);
        game_play_fill_circle(cx, cy, r1 - 3, bg);
        game_play_fill_circle(cx, cy, 2, fg);
        break;
    }
    case 5:
        for (int i = -1; i <= 1; i++)
            game_play_fill_rect(cx - s, cy + i * (s / 2), s * 2, 3, fg);
        break;
    case 6:
        game_play_fill_circle(cx, cy, s, fg);
        game_play_fill_circle(cx, cy, s - 3, bg);
        break;
    default:
        game_play_fill_circle(cx - s / 2, cy, s - 1, fg);
        game_play_fill_circle(cx + s / 2, cy, s - 1, fg);
        break;
    }
}

static void draw_card(int c, int r, bool highlight) {
    int x, y;
    card_rect(c, r, &x, &y);
    const int i = idx(c, r);
    const bool face_up = open[i] || solved[i];

    if (face_up) {
        const int pid = deck[i];
        const uint16_t bg = pair_bg(pid);
        const uint16_t fg = pair_fg(pid);
        const uint16_t border = ui_tint565(bg, -40);
        game_play_fill_round_rect(x, y, card_w, card_h, 6, bg);
        tft.drawRoundRect(PLAY_X + x, PLAY_Y + y, card_w, card_h, 6, border);
        if (highlight)
            game_play_fill_round_rect(x + 2, y + 2, card_w - 4, card_h - 4, 5,
                                      ui_tint565(bg, 22));
        const int sym_sz = card_w < card_h ? card_w / 2 : card_h / 2;
        draw_pair_symbol(pid, x + card_w / 2, y + card_h / 2, sym_sz, fg, bg);
    } else {
        const uint16_t back = ui_rgb565(0x3D2E6Bu);
        const uint16_t back_hi = ui_rgb565(0x5240A0u);
        game_play_fill_round_rect(x, y, card_w, card_h, 6, highlight ? back_hi : back);
        game_play_fill_rect(x + 5, y + 5, card_w - 10, card_h - 10, ui_tint565(back, 12));
        game_play_fill_circle(x + card_w / 2, y + card_h / 2, card_w / 6, ui_tint565(back_hi, 20));
    }
}

static void memoria_redraw() {
    game_play_clear(COL_BG);
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            draw_card(c, r, false);
}

static int hit_card(int px, int py) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            int x, y;
            card_rect(c, r, &x, &y);
            if (px >= x && px < x + card_w && py >= y && py < y + card_h)
                return idx(c, r);
        }
    }
    return -1;
}

static void memoria_reset(GameHud* hud) {
    shuffle_deck();
    memset(open, 0, sizeof(open));
    memset(solved, 0, sizeof(solved));
    first_pick = -1;
    second_pick = -1;
    moves = 0;
    match_streak = 0;
    input_lock = false;
    memoria_redraw();
    game_hud_set_score(hud, 0);
}

static void memoria_init(GameHud* hud) {
    card_w = (PLAY_W - CARD_GAP * (COLS + 1)) / COLS;
    card_h = (PLAY_H - CARD_GAP * (ROWS + 1)) / ROWS;
    memoria_reset(hud);
}

void game_memoria_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_NONE, false);
    game_hud_set_score_tag(hud, "Jog");
    hud->score_lower_better = true;

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        hud->score_lower_better = true;
        memoria_init(hud);

        GameInput in;
        bool dead = false;
        bool won = false;

        while (!dead && !won) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                memoria_redraw();

            if (input_lock && millis() >= lock_until) {
                open[first_pick] = false;
                open[second_pick] = false;
                first_pick = -1;
                second_pick = -1;
                input_lock = false;
                memoria_redraw();
            }

            if (!input_lock && in.just_pressed && in.y >= PLAY_Y) {
                const int pick = hit_card(in.play_x, in.play_y);
                if (pick < 0 || open[pick] || solved[pick]) continue;

                open[pick] = true;
                audio_play(SFX_FLIP);
                game_frame_draw_now();
                draw_card(pick % COLS, pick / COLS, true);

                if (first_pick < 0) {
                    first_pick = pick;
                } else {
                    moves++;
                    game_hud_set_score(hud, moves);
                    second_pick = pick;
                    if (deck[first_pick] == deck[pick]) {
                        match_streak++;
                        audio_play(match_streak >= 3 ? SFX_RECORD : SFX_MATCH);
                        solved[first_pick] = solved[pick] = true;
                        open[first_pick] = open[pick] = false;
                        first_pick = -1;
                        second_pick = -1;
                        memoria_redraw();
                        bool all = true;
                        for (int k = 0; k < COLS * ROWS; k++)
                            if (!solved[k]) all = false;
                        if (all) {
                            won = true;
                            audio_play(SFX_WIN);
                        }
                    } else {
                        match_streak = 0;
                        audio_play(SFX_MISS);
                        input_lock = true;
                        lock_until = millis() + LOCK_MS;
                    }
                }
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, moves, won) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
