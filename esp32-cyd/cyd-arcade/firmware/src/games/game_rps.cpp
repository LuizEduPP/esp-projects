#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "score_store.h"
#include "hw_config.h"
#include "ui_theme.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>

#define COL_BG ui_rgb565(0x0B0F1A)

static const char* CHOICES[] = {"Pedra", "Papel", "Tesoura"};
static const uint32_t COL_CH[] = {0x7A8A9E, 0x00D4FF, 0xF87171};

static int score;
static int btn_y, btn_w, btn_h, gap;

static void layout_buttons() {
    btn_h = 44;
    gap = 10;
    btn_w = (PLAY_W - UI_PAD * 2 - gap * 2) / 3;
    btn_y = PLAY_H / 2 + 20;
}

static int hit_choice(int16_t tx, int16_t ty) {
    if (ty < btn_y || ty >= btn_y + btn_h) return -1;
    for (int i = 0; i < 3; i++) {
        const int x = UI_PAD + i * (btn_w + gap);
        if (tx >= x && tx < x + btn_w) return i;
    }
    return -1;
}

static void draw_buttons(int highlight) {
    layout_buttons();
    game_play_clear(COL_BG);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(ui_theme_get()->text_hi, COL_BG);
    tft.drawString("Escolha:", SCREEN_CX, PLAY_Y + btn_y - 28, 2);
    for (int i = 0; i < 3; i++) {
        const int x = UI_PAD + i * (btn_w + gap);
        const uint16_t bg = (i == highlight) ? ui_theme_get()->accent : ui_theme_get()->card;
        const uint16_t fg = (i == highlight) ? ui_theme_get()->text_hi : ui_theme_get()->text_mute;
        game_play_fill_round_rect(x, btn_y, btn_w, btn_h, 8, bg);
        tft.setTextColor(fg, bg);
        tft.drawString(CHOICES[i], PLAY_X + x + btn_w / 2, PLAY_Y + btn_y + btn_h / 2, 1);
    }
}

static int cpu_pick_weak(int player) {
    if (random(0, 100) < 55)
        return random(0, 3);
    return (player + 1 + random(0, 2)) % 3;
}

static int rps_winner(int a, int b) {
    if (a == b) return 0;
    if ((a + 1) % 3 == b) return 2;
    return 1;
}

void game_rps_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin("RPS", "rps", 0xFBBF24);
    if (!hud) return;

    score = 0;
    game_hud_set_score(hud, 0);
    draw_buttons(-1);

    GameInput in;
    for (;;) {
        game_frame_tick();
        game_input_poll(&in);
        if (game_hud_poll(hud)) {
            if (score > 0) score_store_save(hud->engine, score);
            game_hud_end(hud);
            return;
        }
        if (game_hud_consume_resume_redraw(hud))
            draw_buttons(-1);

        if (in.just_pressed && in.y >= PLAY_Y) {
            const int pick = hit_choice(in.play_x, in.play_y);
            if (pick < 0) continue;

            const int cpu = cpu_pick_weak(pick);
            draw_buttons(pick);

            char msg[32];
            const int w = rps_winner(pick, cpu);
            if (w == 1) {
                score += 10;
                snprintf(msg, sizeof(msg), "Venceu! CPU:%s", CHOICES[cpu]);
            } else if (w == 2) {
                snprintf(msg, sizeof(msg), "Perdeu CPU:%s", CHOICES[cpu]);
            } else {
                score += 2;
                snprintf(msg, sizeof(msg), "Empate");
            }
            game_hud_set_score(hud, score);

            tft.setTextDatum(TC_DATUM);
            tft.setTextColor(ui_rgb565(COL_CH[pick]), COL_BG);
            tft.drawString(CHOICES[pick], SCREEN_CX, PLAY_Y + 40, 2);
            tft.setTextColor(ui_theme_get()->text_mute, COL_BG);
            tft.drawString("vs", SCREEN_CX, PLAY_Y + 62, 1);
            tft.setTextColor(ui_rgb565(COL_CH[cpu]), COL_BG);
            tft.drawString(CHOICES[cpu], SCREEN_CX, PLAY_Y + 80, 2);
            tft.setTextColor(w == 1 ? ui_theme_get()->ok :
                             w == 2 ? ui_theme_get()->danger : ui_theme_get()->text_hi, COL_BG);
            tft.drawString(msg, SCREEN_CX, PLAY_Y + 110, 2);

            delay(1400);
            draw_buttons(-1);
        }
        game_frame_delay();
    }
}
