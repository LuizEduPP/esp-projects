#include "game_play.h"
#include "game_input.h"
#include "display.h"
#include "hw_config.h"
#include "score_store.h"
#include "touch_input.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_keyboard.h"
#include "ui_theme.h"
#include "buzzer.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TH ui_theme_get()

static GameHud* s_active_hud;

static void draw_status_bar(GameHud* hud) {
    tft.fillRect(0, 0, SCREEN_W, STATUS_H, TH->bg);
    tft.fillRect(0, STATUS_H - 2, SCREEN_W, 2, TH->border);

    tft.fillCircle(14, STATUS_H / 2, 5, TH->accent);

    if (hud->level > 0 || hud->level_prefix == 'V' || hud->level_prefix == 'C') {
        char lv[8];
        const char p = hud->level_prefix ? hud->level_prefix : 'F';
        snprintf(lv, sizeof(lv), "%c%d", p, hud->level);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->accent, TH->bg);
        tft.drawString(lv, SCREEN_CX, STATUS_H / 2, 2);
    }

    char sc[16];
    snprintf(sc, sizeof(sc), "%d", hud->score);
    tft.fillRoundRect(HUD_SCORE_X, 4, 54, 20, 4, TH->card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->pal[1], TH->card);
    tft.drawString(sc, HUD_SCORE_X + 27, STATUS_H / 2, 2);

    tft.fillRoundRect(HUD_PAUSE_X, 4, HUD_PAUSE_W, 20, 4, TH->card);
    ui_icon_draw(HUD_PAUSE_X + 4, 2, 20, UI_ICON_PAUSE, TH->text_hi);
}

static bool clip_play_rect(int* x, int* y, int* w, int* h) {
    if (*w <= 0 || *h <= 0) return false;
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > PLAY_W) *w = PLAY_W - *x;
    if (*y + *h > PLAY_H) *h = PLAY_H - *y;
    return *w > 0 && *h > 0;
}

static void refresh_status_if_clipped(bool clipped_top) {
    if (clipped_top && s_active_hud)
        draw_status_bar(s_active_hud);
}

static bool wait_pause_choice(int btn1_y, int btn2_y, int btn_w, int btn_h) {
    const int bx = UI_PAD;
    touch_wait_release();
    for (;;) {
        touch_poll();
        if (!touch_is_pressed()) {
            delay(10);
            continue;
        }
        const int16_t tx = touch_get_x();
        const int16_t ty = touch_get_y();
        touch_wait_release();
        if (tx >= bx && tx < bx + btn_w) {
            if (ty >= btn1_y && ty < btn1_y + btn_h) return false;
            if (ty >= btn2_y && ty < btn2_y + btn_h) return true;
        }
        delay(10);
    }
}

static bool show_pause_menu() {
    const int ox = UI_PAD;
    const int ow = UI_CONTENT_W;
    const int oy = PLAY_Y + PLAY_H / 2 - 72;
    const int btn_h = 40;
    const int btn1_y = oy + 56;
    const int btn2_y = oy + 104;

    tft.fillRoundRect(ox, oy, ow, 148, 10, TH->card);
    tft.drawRoundRect(ox, oy, ow, 148, 10, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("Pausado", SCREEN_CX, oy + 24, 2);

    tft.fillRoundRect(ox, btn1_y, ow, btn_h, 8, TH->accent);
    tft.setTextColor(TH->bg, TH->accent);
    tft.drawString("Continuar", SCREEN_CX, btn1_y + btn_h / 2, 2);

    tft.fillRoundRect(ox, btn2_y, ow, btn_h, 8, TH->surface);
    tft.drawRoundRect(ox, btn2_y, ow, btn_h, 8, TH->border);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString("Voltar", SCREEN_CX, btn2_y + btn_h / 2, 2);

    return wait_pause_choice(btn1_y, btn2_y, ow, btn_h);
}

static GameEndAction wait_end_choice(int btn1_y, int btn2_y, int btn_w, int btn1_h, int btn2_h) {
    const int bx = UI_PAD;
    touch_wait_release();
    for (;;) {
        touch_poll();
        if (!touch_is_pressed()) {
            delay(10);
            continue;
        }
        const int16_t tx = touch_get_x();
        const int16_t ty = touch_get_y();
        touch_wait_release();
        if (tx >= bx && tx < bx + btn_w) {
            if (ty >= btn1_y && ty < btn1_y + btn1_h) return GAME_END_RETRY;
            if (ty >= btn2_y && ty < btn2_y + btn2_h) return GAME_END_MENU;
        }
        delay(10);
    }
}

GameHud* game_hud_begin(const char* title, const char* engine, uint32_t color) {
    auto* hud = (GameHud*)calloc(1, sizeof(GameHud));
    if (!hud) return nullptr;

    strncpy(hud->title, title, sizeof(hud->title) - 1);
    strncpy(hud->engine, engine, sizeof(hud->engine) - 1);
    hud->accent_color = ui_rgb565(color);
    hud->level_prefix = 'F';
    hud->best = score_store_get(engine);
    score_store_get_name(engine, hud->best_name, sizeof(hud->best_name));
    hud->pause_after_ms = millis() + 450;

    s_active_hud = hud;
    ui_fill_screen_bg();
    tft.fillRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, TH->play_bg);
    draw_status_bar(hud);
    touch_wait_release();

    Serial.printf("[HUD] %s best=%d heap=%u\n", title, hud->best, ESP.getFreeHeap());
    return hud;
}

void game_hud_end(GameHud* hud) {
    if (s_active_hud == hud) s_active_hud = nullptr;
    free(hud);
}

void game_hud_set_score(GameHud* hud, int score) {
    if (!hud || hud->score == score) return;
    hud->score = score;
    draw_status_bar(hud);
}

void game_hud_set_level(GameHud* hud, int level) {
    if (!hud || hud->level == level) return;
    hud->level = level;
    draw_status_bar(hud);
}

void game_hud_set_level_prefix(GameHud* hud, char prefix) {
    if (!hud) return;
    hud->level_prefix = prefix;
}

void game_hud_reset_play(GameHud* hud) {
    if (!hud) return;
    hud->score = 0;
    hud->level = 0;
    tft.fillRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, TH->play_bg);
    draw_status_bar(hud);
    touch_wait_release();
}

bool game_hud_poll(GameHud* hud) {
    if (!hud) return true;
    if (hud->quit) return true;

    if (millis() >= hud->pause_after_ms && touch_is_pressed()) {
        const int16_t tx = touch_get_x();
        const int16_t ty = touch_get_y();
        if (tx >= HUD_PAUSE_X && tx < HUD_PAUSE_X + HUD_PAUSE_W && ty < STATUS_H) {
            touch_wait_release();
            if (show_pause_menu()) {
                hud->quit = true;
                return true;
            }
            hud->resume_redraw = true;
            draw_status_bar(hud);
            return false;
        }
    }
    return false;
}

bool game_hud_consume_resume_redraw(GameHud* hud) {
    if (!hud || !hud->resume_redraw) return false;
    hud->resume_redraw = false;
    return true;
}

GameEndAction game_hud_end_game(GameHud* hud, int score, bool won) {
    if (!hud) return GAME_END_MENU;

    const bool record = score_store_save(hud->engine, score);
    if (record && score > 0)
        buzzer_play(SFX_RECORD);
    else if (won)
        buzzer_play(SFX_WIN);
    else
        buzzer_play(SFX_LOSE);

    hud->best = score_store_get(hud->engine);
    score_store_get_name(hud->engine, hud->best_name, sizeof(hud->best_name));

    if (record && score > 0) {
        char name[SCORE_NAME_LEN + 1];
        strncpy(name, hud->best_name, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
        if (!name[0]) strncpy(name, "PLAYER", sizeof(name) - 1);
        if (ui_keyboard_enter_name("NOVO RECORDE!", score, name, sizeof(name)))
            score_store_set_name(hud->engine, name);
        strncpy(hud->best_name, name, sizeof(hud->best_name) - 1);
        hud->best = score_store_get(hud->engine);
    }

    tft.fillRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, TH->play_bg);

    const int ox = UI_PAD;
    const int oy = PLAY_Y + 32;
    const int ow = UI_CONTENT_W;
    const int oh = 116;
    const int btn_w = UI_CONTENT_W;
    const int btn1_y = PLAY_Y + 164;
    const int btn1_h = 44;
    const int btn2_y = PLAY_Y + 216;
    const int btn2_h = 40;

    tft.fillRoundRect(ox, oy, ow, oh, 10, TH->card);
    if (record)
        tft.drawRoundRect(ox, oy, ow, oh, 10, TH->pal[1]);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(won ? TH->ok : TH->danger, TH->card);
    tft.drawString(won ? "Vitoria!" : "Fim de jogo", SCREEN_CX, oy + 26, 2);

    char buf[32];
    snprintf(buf, sizeof(buf), "Pontos %d", score);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(buf, SCREEN_CX, oy + 54, 2);

    if (record && score > 0) {
        tft.setTextColor(TH->pal[1], TH->card);
        tft.drawString("Novo recorde!", SCREEN_CX, oy + 74, 1);
    }

    if (hud->best > 0) {
        if (hud->best_name[0])
            snprintf(buf, sizeof(buf), "%s %d", hud->best_name, hud->best);
        else
            snprintf(buf, sizeof(buf), "Recorde %d", hud->best);
        tft.setTextColor(TH->text_mute, TH->card);
        tft.drawString(buf, SCREEN_CX, oy + 94, 1);
    }

    tft.fillRoundRect(ox, btn1_y, btn_w, btn1_h, 8, TH->accent);
    tft.setTextColor(TH->bg, TH->accent);
    tft.drawString("Novamente", SCREEN_CX, btn1_y + btn1_h / 2, 2);

    tft.fillRoundRect(ox, btn2_y, btn_w, btn2_h, 8, TH->card);
    tft.drawRoundRect(ox, btn2_y, btn_w, btn2_h, 8, TH->border);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("Menu", SCREEN_CX, btn2_y + btn2_h / 2, 2);

    return wait_end_choice(btn1_y, btn2_y, btn_w, btn1_h, btn2_h);
}

void game_play_toast(const char* title, const char* sub, uint16_t stroke, uint16_t bg) {
    (void)stroke;
    const int w = 156;
    const int h = sub && sub[0] ? 48 : 32;
    const int x = (PLAY_W - w) / 2;
    const int y = PLAY_H / 2 - h / 2;
    game_play_fill_rect(x, y, w, h, bg);
    tft.fillRoundRect(PLAY_X + x, PLAY_Y + y, w, h, 6, TH->card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(title, PLAY_X + x + w / 2, PLAY_Y + y + (sub && sub[0] ? h / 2 - 6 : h / 2), 2);
    if (sub && sub[0]) {
        tft.setTextColor(TH->text_mute, TH->card);
        tft.drawString(sub, PLAY_X + x + w / 2, PLAY_Y + y + h / 2 + 10, 1);
    }
}

void game_play_clear(uint16_t color) {
    game_frame_draw_now();
    tft.fillRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, color);
    if (s_active_hud) draw_status_bar(s_active_hud);
}

void game_play_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (!game_frame_draw_on()) return;
    const bool clipped_top = (y < 0);
    if (!clip_play_rect(&x, &y, &w, &h)) {
        refresh_status_if_clipped(clipped_top);
        return;
    }
    tft.fillRect(PLAY_X + x, PLAY_Y + y, w, h, color);
    refresh_status_if_clipped(clipped_top);
}

void game_play_fill_round_rect(int x, int y, int w, int h, int r, uint16_t color) {
    if (!game_frame_draw_on()) return;
    const bool clipped_top = (y < 0);
    if (!clip_play_rect(&x, &y, &w, &h)) {
        refresh_status_if_clipped(clipped_top);
        return;
    }
    tft.fillRoundRect(PLAY_X + x, PLAY_Y + y, w, h, r, color);
    refresh_status_if_clipped(clipped_top);
}

void game_play_fill_circle(int cx, int cy, int r, uint16_t color) {
    if (!game_frame_draw_on()) return;
    if (r <= 0) return;
    const bool clipped_top = (cy - r < 0);
    tft.fillCircle(PLAY_X + cx, PLAY_Y + cy, r, color);
    refresh_status_if_clipped(clipped_top);
}

void game_play_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint16_t color) {
    if (!game_frame_draw_on()) return;
    tft.fillTriangle(PLAY_X + x0, PLAY_Y + y0,
                     PLAY_X + x1, PLAY_Y + y1,
                     PLAY_X + x2, PLAY_Y + y2, color);
}

void game_play_hint(const char* msg, uint16_t fg, uint16_t bg) {
    (void)fg;
    game_play_fill_rect(0, PLAY_H / 2 - 20, PLAY_W, 40, bg);
    tft.fillRoundRect(PLAY_X + UI_PAD, PLAY_Y + PLAY_H / 2 - 18, UI_CONTENT_W, 36, 6, TH->card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->pal[1], TH->card);
    tft.drawString(msg, SCREEN_CX, PLAY_Y + PLAY_H / 2, 2);
}

void game_play_clear_hint(uint16_t bg) {
    game_play_fill_rect(0, PLAY_H / 2 - 20, PLAY_W, 40, bg);
}
