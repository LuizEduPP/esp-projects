#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "ui_draw.h"
#include <Arduino.h>
#include <string.h>

#define COLS 4
#define ROWS 4
#define PAIRS 8
#define CARD_GAP 6
#define COL_BG 0x0000
#define COL_BACK 0x4A69
#define COL_BACK_HI 0x6B8E
#define LOCK_MS 750

static const uint16_t PAIR_COL[PAIRS] = {
    0xF800, 0x07E0, 0x001F, 0xFFE0, 0xF81F, 0x07FF, 0xFD20, 0xFF80,
};

static uint8_t deck[COLS * ROWS];
static bool open[COLS * ROWS];
static bool solved[COLS * ROWS];
static int first_pick;
static int second_pick;
static int moves;
static bool input_lock;
static uint32_t lock_until;
static int card_w, card_h;

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

static void draw_card(int c, int r, bool highlight) {
    int x, y;
    card_rect(c, r, &x, &y);
    const int i = idx(c, r);
    const bool face_up = open[i] || solved[i];

    if (face_up) {
        const uint16_t col = PAIR_COL[deck[i]];
        game_play_fill_round_rect(x, y, card_w, card_h, 6, col);
        if (highlight)
            game_play_fill_round_rect(x + 2, y + 2, card_w - 4, card_h - 4, 5, ui_tint565(col, 25));
        game_play_fill_circle(x + card_w / 2, y + card_h / 2, card_w / 5, ui_tint565(col, -35));
        game_play_fill_circle(x + card_w / 2, y + card_h / 2, card_w / 8, 0xFFFF);
    } else {
        game_play_fill_round_rect(x, y, card_w, card_h, 6, highlight ? COL_BACK_HI : COL_BACK);
        game_play_fill_rect(x + 6, y + 6, card_w - 12, card_h - 12, ui_tint565(COL_BACK, 15));
        for (int dy = 8; dy < card_h - 8; dy += 10)
            game_play_fill_rect(x + 8, y + dy, card_w - 16, 2, ui_tint565(COL_BACK_HI, -20));
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
                buzzer_play(SFX_FLIP);
                game_frame_draw_now();
                draw_card(pick % COLS, pick / COLS, true);

                if (first_pick < 0) {
                    first_pick = pick;
                } else {
                    moves++;
                    game_hud_set_score(hud, moves);
                    second_pick = pick;
                    if (deck[first_pick] == deck[pick]) {
                        buzzer_play(SFX_MATCH);
                        solved[first_pick] = solved[pick] = true;
                        open[first_pick] = open[pick] = false;
                        first_pick = -1;
                        second_pick = -1;
                        bool all = true;
                        for (int i = 0; i < COLS * ROWS; i++)
                            if (!solved[i]) all = false;
                        if (all) {
                            won = true;
                            buzzer_play(SFX_WIN);
                        }
                    } else {
                        buzzer_play(SFX_MISS);
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
