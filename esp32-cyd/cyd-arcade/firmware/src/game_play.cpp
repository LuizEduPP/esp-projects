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

uint16_t game_play_field_bg(void) {
    return TH->play_field;
}

static void fill_play_field(void) {
    tft.fillRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, TH->play_field);
    if (PLAY_MARGIN > 0) {
        tft.drawRect(PLAY_X + PLAY_MARGIN - 1, PLAY_Y + PLAY_MARGIN - 1,
                     PLAY_W - (PLAY_MARGIN - 1) * 2, PLAY_H - (PLAY_MARGIN - 1) * 2,
                     TH->play_grid);
    }
}

static void draw_lives_in_status(GameHud* hud) {
    if (!hud || hud->lives_max <= 0) return;
    tft.fillRoundRect(HUD_LIVES_X, HUD_BADGE_Y, HUD_LIVES_W, UI_BADGE_H, UI_BADGE_R, TH->card);
    tft.drawRoundRect(HUD_LIVES_X, HUD_BADGE_Y, HUD_LIVES_W, UI_BADGE_H, UI_BADGE_R, TH->border);
    for (int i = 0; i < hud->lives_max; i++) {
        const int cx = HUD_LIVES_X + 10 + i * 12;
        const int cy = HUD_BADGE_Y + UI_BADGE_H / 2;
        const bool on = i < hud->lives;
        tft.fillCircle(cx, cy, 5, on ? TH->life_on : TH->life_off);
        if (on)
            tft.drawCircle(cx, cy, 5, TH->text_hi);
    }
}

static const char* tier_label(HudTierKind kind) {
    switch (kind) {
    case HUD_TIER_FASE:  return "Fase";
    case HUD_TIER_NIVEL: return "Niv";
    case HUD_TIER_CPU:   return "CPU";
    default: return nullptr;
    }
}

static bool tier_visible(const GameHud* hud) {
    if (!hud || hud->tier_kind == HUD_TIER_NONE) return false;
    if (hud->tier > 0) return true;
    return hud->tier_show_zero;
}

static int hud_tier_x(const GameHud* hud) {
    return (hud && hud->lives_max > 0) ? HUD_TIER_X : HUD_LIVES_X;
}

static int hud_score_x(const GameHud* hud) {
    if (hud && hud->lives_max <= 0 && !tier_visible(hud))
        return HUD_LIVES_X;
    if (hud && hud->lives_max <= 0)
        return HUD_TIER_X + HUD_TIER_W + 4;
    if (!tier_visible(hud))
        return HUD_TIER_X;
    return HUD_SCORE_X;
}

static void draw_tier_badge(GameHud* hud) {
    if (!tier_visible(hud)) return;
    const char* word = tier_label(hud->tier_kind);
    if (!word) return;

    char buf[12];
    snprintf(buf, sizeof(buf), "%s %d", word, hud->tier);
    const int x = hud_tier_x(hud);
    tft.fillRoundRect(x, HUD_BADGE_Y, HUD_TIER_W, UI_BADGE_H, UI_BADGE_R, TH->card);
    tft.drawRoundRect(x, HUD_BADGE_Y, HUD_TIER_W, UI_BADGE_H, UI_BADGE_R, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->accent, TH->card);
    tft.drawString(buf, x + HUD_TIER_W / 2, STATUS_H / 2, 2);
}

static void draw_score_badge(GameHud* hud) {
    if (!hud || !hud->score_visible) return;
    char sc[16];
    if (hud->score_tag[0])
        snprintf(sc, sizeof(sc), "%s %d", hud->score_tag, hud->score);
    else
        snprintf(sc, sizeof(sc), "%d", hud->score);

    const int x = hud_score_x(hud);
    tft.fillRoundRect(x, HUD_BADGE_Y, HUD_SCORE_W, UI_BADGE_H, UI_BADGE_R, TH->card);
    tft.drawRoundRect(x, HUD_BADGE_Y, HUD_SCORE_W, UI_BADGE_H, UI_BADGE_R, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(sc, x + HUD_SCORE_W / 2, STATUS_H / 2, 2);
}

static void draw_status_bar(GameHud* hud) {
    tft.fillRect(0, 0, SCREEN_W, STATUS_H, TH->surface);
    tft.drawFastHLine(0, STATUS_H - 1, SCREEN_W, TH->border);

    draw_lives_in_status(hud);
    draw_tier_badge(hud);
    draw_score_badge(hud);

    tft.fillRoundRect(HUD_PAUSE_X, HUD_BADGE_Y, HUD_PAUSE_W, UI_BADGE_H, UI_BADGE_R, TH->card);
    tft.drawRoundRect(HUD_PAUSE_X, HUD_BADGE_Y, HUD_PAUSE_W, UI_BADGE_H, UI_BADGE_R, TH->border);
    ui_icon_draw(HUD_PAUSE_X + 5, HUD_BADGE_Y, 20, UI_ICON_PAUSE, TH->text_hi);
}

static bool clip_play_rect(int* x, int* y, int* w, int* h) {
    if (*w <= 0 || *h <= 0) return false;
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x + *w > PLAY_W) *w = PLAY_W - *x;
    if (*y + *h > PLAY_H) *h = PLAY_H - *y;
    return *w > 0 && *h > 0;
}

static void refresh_status_if_overlap(int play_y, int play_h) {
    if (!s_active_hud || play_h <= 0) return;
    const int screen_y0 = PLAY_Y + play_y;
    const int screen_y1 = screen_y0 + play_h;
    if (screen_y1 > 0 && screen_y0 < STATUS_H)
        draw_status_bar(s_active_hud);
}

static bool wait_modal_choice(int btn_x, int btn1_y, int btn2_y, int btn_w, int btn_h) {
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
        if (tx >= btn_x && tx < btn_x + btn_w) {
            if (ty >= btn1_y && ty < btn1_y + btn_h) return false;
            if (ty >= btn2_y && ty < btn2_y + btn_h) return true;
        }
        delay(10);
    }
}

static GameEndAction wait_end_choice(int btn_x, int btn1_y, int btn2_y, int btn_w, int btn_h) {
    return wait_modal_choice(btn_x, btn1_y, btn2_y, btn_w, btn_h)
               ? GAME_END_MENU
               : GAME_END_RETRY;
}

static void draw_modal_scrim() {
    const uint16_t scrim = ui_tint565(TH->bg, -48);
    tft.fillRect(0, 0, SCREEN_W, STATUS_H, ui_tint565(TH->surface, -30));
    tft.fillRect(0, PLAY_Y, SCREEN_W, PLAY_H, scrim);
}

static void draw_modal_shell(int ox, int oy, int ow, int oh, uint16_t ring_col, bool bordered) {
    tft.fillRoundRect(ox + 2, oy + 4, ow, oh, UI_MODAL_R, ui_tint565(TH->bg, -70));
    tft.fillRoundRect(ox, oy, ow, oh, UI_MODAL_R, TH->card);
    if (!bordered) return;
    tft.drawRoundRect(ox, oy, ow, oh, UI_MODAL_R, TH->border);
    tft.drawRoundRect(ox + 1, oy + 1, ow - 2, oh - 2, UI_MODAL_R - 1, ring_col);
}

static void draw_modal_header(int ox, int oy, int ow, UiIcon icon, uint16_t icon_col,
                              const char* title, const char* subtitle) {
    const int hdr_h = 52;
    tft.fillRoundRect(ox, oy, ow, hdr_h + UI_MODAL_R, UI_MODAL_R, TH->surface);
    tft.fillRect(ox, oy + hdr_h, ow, UI_MODAL_R, TH->surface);
    tft.drawFastHLine(ox + UI_MODAL_R, oy + hdr_h, ow - UI_MODAL_R * 2, TH->border);
    ui_icon_draw(ox + 14, oy + 14, 24, icon, icon_col);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(title, ox + 46, oy + 14, 2);
    if (subtitle && subtitle[0]) {
        tft.setTextColor(TH->text_mute, TH->surface);
        tft.drawString(subtitle, ox + 46, oy + 34, 1);
    }
}

static void draw_modal_btn(int x, int y, int w, int h, UiIcon icon,
                           const char* label, uint16_t fill, uint16_t stroke,
                           uint16_t fg, uint16_t icon_col) {
    tft.fillRoundRect(x, y, w, h, UI_CARD_R, fill);
    if (stroke != fill)
        tft.drawRoundRect(x, y, w, h, UI_CARD_R, stroke);
    ui_icon_draw(x + 12, y + (h - 20) / 2, 20, icon, icon_col);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(fg, fill);
    tft.drawString(label, x + 40, y + h / 2, 2);
}

static void draw_modal_actions(int ox, int oy, int ow, int oh,
                               const char* btn1, UiIcon icon1,
                               const char* btn2, UiIcon icon2,
                               int* out_btn_x, int* out_btn1_y, int* out_btn2_y,
                               int* out_btn_w, int* out_btn_h) {
    const int btn_w = ow - 28;
    const int btn_h = UI_BTN_H;
    const int btn_x = ox + 14;
    const int btn2_y = oy + oh - 16 - btn_h;
    const int btn1_y = btn2_y - btn_h - 10;

    draw_modal_btn(btn_x, btn1_y, btn_w, btn_h, icon1, btn1,
                   TH->accent, TH->accent, TH->bg, TH->bg);
    draw_modal_btn(btn_x, btn2_y, btn_w, btn_h, icon2, btn2,
                   TH->surface, TH->border, TH->text_hi, TH->text_mute);

    *out_btn_x = btn_x;
    *out_btn1_y = btn1_y;
    *out_btn2_y = btn2_y;
    *out_btn_w = btn_w;
    *out_btn_h = btn_h;
}

static bool show_pause_menu() {
    draw_modal_scrim();

    const int ow = 204;
    const int oh = 168;
    const int ox = (SCREEN_W - ow) / 2;
    const int oy = PLAY_Y + (PLAY_H - oh) / 2;

    draw_modal_shell(ox, oy, ow, oh, TH->accent, false);
    draw_modal_header(ox, oy, ow, UI_ICON_PAUSE, TH->accent_hi,
                      "Pausado", "Toque para retomar");

    int btn_x, btn1_y, btn2_y, btn_w, btn_h;
    draw_modal_actions(ox, oy, ow, oh, "Continuar", UI_ICON_PLAY, "Menu", UI_ICON_EXIT,
                       &btn_x, &btn1_y, &btn2_y, &btn_w, &btn_h);

    return wait_modal_choice(btn_x, btn1_y, btn2_y, btn_w, btn_h);
}

GameHud* game_hud_begin(const char* engine) {
    auto* hud = (GameHud*)calloc(1, sizeof(GameHud));
    if (!hud) return nullptr;

    strncpy(hud->engine, engine, sizeof(hud->engine) - 1);
    strncpy(hud->score_tag, "Pts", sizeof(hud->score_tag));
    hud->tier_kind = HUD_TIER_FASE;
    hud->score_visible = true;
    hud->best = score_store_get(engine);
    score_store_get_name(engine, hud->best_name, sizeof(hud->best_name));
    hud->pause_after_ms = millis() + 450;

    s_active_hud = hud;
    ui_fill_screen_bg();
    fill_play_field();
    draw_status_bar(hud);
    touch_wait_release();

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

void game_hud_set_score_tag(GameHud* hud, const char* tag) {
    if (!hud || !tag) return;
    strncpy(hud->score_tag, tag, sizeof(hud->score_tag) - 1);
    hud->score_tag[sizeof(hud->score_tag) - 1] = '\0';
    draw_status_bar(hud);
}

void game_hud_set_score_visible(GameHud* hud, bool visible) {
    if (!hud || hud->score_visible == visible) return;
    hud->score_visible = visible;
    draw_status_bar(hud);
}

void game_hud_set_tier_mode(GameHud* hud, HudTierKind kind, bool show_at_zero) {
    if (!hud) return;
    hud->tier_kind = kind;
    hud->tier_show_zero = show_at_zero;
    draw_status_bar(hud);
}

void game_hud_set_tier(GameHud* hud, int value) {
    if (!hud || hud->tier == value) return;
    hud->tier = value;
    draw_status_bar(hud);
}

static void game_hud_toast_tick(GameHud* hud) {
    if (!hud || hud->toast_until_ms == 0) return;
    if ((int32_t)(millis() - hud->toast_until_ms) < 0) return;
    game_play_fill_rect(hud->toast_x, hud->toast_y, hud->toast_w, hud->toast_h, game_play_field_bg());
    hud->toast_until_ms = 0;
    hud->resume_redraw = true;
}

static void game_hud_show_tier_toast(GameHud* hud, const char* title) {
    const int w = 118;
    const int h = 24;
    const int x = (PLAY_W - w) / 2;
    const int y = 6;
    game_play_fill_round_rect(x, y, w, h, 5, TH->card);
    tft.drawRoundRect(PLAY_X + x, PLAY_Y + y, w, h, 5, TH->accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->accent_hi, TH->card);
    tft.drawString(title, PLAY_X + x + w / 2, PLAY_Y + y + h / 2, 2);
    hud->toast_x = (int16_t)x;
    hud->toast_y = (int16_t)y;
    hud->toast_w = (int16_t)w;
    hud->toast_h = (int16_t)h;
    hud->toast_until_ms = millis() + 1400;
}

void game_hud_show_toast(GameHud* hud, const char* title) {
    if (!hud || !title) return;
    game_hud_show_tier_toast(hud, title);
}

bool game_hud_advance_tier(GameHud* hud, int new_tier) {
    if (!hud || hud->tier == new_tier) return false;

    const char* word = tier_label(hud->tier_kind);
    if (!word) word = "Fase";

    char buf[14];
    snprintf(buf, sizeof(buf), "%s %d", word, new_tier);

    hud->tier = new_tier;
    draw_status_bar(hud);
    game_hud_show_tier_toast(hud, buf);
    buzzer_play(SFX_LEVEL);
    return false;
}

void game_hud_set_lives(GameHud* hud, int lives, int max_lives) {
    if (!hud) return;
    if (max_lives < 0) max_lives = 0;
    if (lives < 0) lives = 0;
    if (max_lives > 0 && lives > max_lives) lives = max_lives;
    hud->lives = lives;
    hud->lives_max = max_lives;
    draw_status_bar(hud);
}

void game_hud_reset_play(GameHud* hud) {
    if (!hud) return;
    hud->score = 0;
    hud->tier = 0;
    hud->lives = 0;
    hud->lives_max = 0;
    fill_play_field();
    draw_status_bar(hud);
    touch_wait_release();
}

bool game_hud_poll(GameHud* hud) {
    if (!hud) return true;
    game_hud_toast_tick(hud);
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

    const bool record = hud->score_lower_better
                            ? score_store_save_lower(hud->engine, score)
                            : score_store_save(hud->engine, score);
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

    fill_play_field();
    draw_modal_scrim();

    const bool show_best = hud->best > 0;
    const bool show_record = record && score > 0;
    const int ow = 204;
    const int oh = show_record ? 218 : (show_best ? 206 : 192);
    const int ox = (SCREEN_W - ow) / 2;
    const int oy = PLAY_Y + (PLAY_H - oh) / 2;
    const uint16_t ring = show_record ? TH->accent_hi : (won ? TH->ok : TH->danger);

    draw_modal_shell(ox, oy, ow, oh, ring, false);
    draw_modal_header(ox, oy, ow,
                      won ? UI_ICON_CHECK : UI_ICON_X,
                      won ? TH->ok : TH->danger,
                      won ? "Vitoria!" : "Fim de jogo",
                      won ? "Boa partida!" : "Tente outra vez");

    char buf[32];
    int body_y = oy + 64;
    tft.setTextDatum(MC_DATUM);
    snprintf(buf, sizeof(buf), "%s %d",
             hud->score_tag[0] ? hud->score_tag : "Pontos", score);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(buf, SCREEN_CX, body_y, 2);

    if (show_record) {
        body_y += 18;
        tft.setTextColor(TH->accent_hi, TH->card);
        tft.drawString("Novo recorde!", SCREEN_CX, body_y, 1);
    }
    if (show_best) {
        body_y += 18;
        if (hud->best_name[0])
            snprintf(buf, sizeof(buf), "%s %d", hud->best_name, hud->best);
        else
            snprintf(buf, sizeof(buf), "Recorde %d", hud->best);
        tft.setTextColor(TH->text_mute, TH->card);
        tft.drawString(buf, SCREEN_CX, body_y, 1);
    }

    int btn_x, btn1_y, btn2_y, btn_w, btn_h;
    draw_modal_actions(ox, oy, ow, oh, "Novamente", UI_ICON_PLAY, "Menu", UI_ICON_EXIT,
                       &btn_x, &btn1_y, &btn2_y, &btn_w, &btn_h);

    return wait_end_choice(btn_x, btn1_y, btn2_y, btn_w, btn_h);
}

void game_play_toast(const char* title, const char* sub, uint16_t stroke, uint16_t bg) {
    (void)stroke;
    const int w = 156;
    const int h = sub && sub[0] ? 48 : 32;
    const int x = (PLAY_W - w) / 2;
    const int y = PLAY_H / 2 - h / 2;
    game_play_fill_rect(x, y, w, h, bg);
    tft.fillRoundRect(PLAY_X + x, PLAY_Y + y, w, h, 6, TH->card);
    tft.drawRoundRect(PLAY_X + x, PLAY_Y + y, w, h, 6, TH->accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(title, PLAY_X + x + w / 2, PLAY_Y + y + (sub && sub[0] ? h / 2 - 8 : h / 2), 2);
    if (sub && sub[0]) {
        tft.setTextColor(TH->accent, TH->card);
        tft.drawString(sub, PLAY_X + x + w / 2, PLAY_Y + y + h / 2 + 12, 2);
    }
}

void game_play_clear(uint16_t color) {
    game_frame_draw_now();
    tft.fillRect(PLAY_X, PLAY_Y, PLAY_W, PLAY_H, color);
    if (s_active_hud)
        draw_status_bar(s_active_hud);
}

void game_play_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (!game_frame_draw_on()) return;
    if (!clip_play_rect(&x, &y, &w, &h)) return;
    tft.fillRect(PLAY_X + x, PLAY_Y + y, w, h, color);
    refresh_status_if_overlap(y, h);
}

void game_play_fill_round_rect(int x, int y, int w, int h, int r, uint16_t color) {
    if (!game_frame_draw_on()) return;
    if (!clip_play_rect(&x, &y, &w, &h)) return;
    tft.fillRoundRect(PLAY_X + x, PLAY_Y + y, w, h, r, color);
    refresh_status_if_overlap(y, h);
}

void game_play_fill_circle(int cx, int cy, int r, uint16_t color) {
    if (!game_frame_draw_on()) return;
    if (r <= 0) return;
    tft.fillCircle(PLAY_X + cx, PLAY_Y + cy, r, color);
    refresh_status_if_overlap(cy - r, r * 2);
}

void game_play_hint(const char* msg, uint16_t fg, uint16_t bg) {
    (void)fg;
    game_play_fill_rect(0, PLAY_H / 2 - 20, PLAY_W, 40, bg);
    tft.fillRoundRect(PLAY_X + UI_PAD, PLAY_Y + PLAY_H / 2 - 18, UI_CONTENT_W, 36, 6, TH->card);
    tft.drawRoundRect(PLAY_X + UI_PAD, PLAY_Y + PLAY_H / 2 - 18, UI_CONTENT_W, 36, 6, TH->accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(msg, SCREEN_CX, PLAY_Y + PLAY_H / 2, 2);
}

void game_play_clear_hint(uint16_t bg) {
    game_play_fill_rect(0, PLAY_H / 2 - 20, PLAY_W, 40, bg);
}
