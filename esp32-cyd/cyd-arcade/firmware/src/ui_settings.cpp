#include "ui_settings.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "touch_input.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>

#define TH ui_theme_get()

struct SettingsBtn {
    int x, y, w, h;
    int id;
};

enum {
    BTN_BRI_MINUS = 1,
    BTN_BRI_PLUS,
    BTN_SOUND_TOGGLE,
    BTN_CALIB,
    BTN_BACK,
};

static SettingsBtn s_btns[5];
static int s_btn_count;

static const int BAR_X = UI_PAD + 44;
static const int BAR_W = SCREEN_W - UI_PAD * 2 - 88;
static const int BAR_H = 24;
static const int BRI_BAR_Y = 96;
static const int VOL_BAR_Y = 156;

static void add_btn(int x, int y, int w, int h, int id) {
    if (s_btn_count >= (int)(sizeof(s_btns) / sizeof(s_btns[0]))) return;
    s_btns[s_btn_count++] = {x, y, w, h, id};
}

static void build_layout() {
    s_btn_count = 0;
    add_btn(UI_PAD, BRI_BAR_Y - 6, 36, 36, BTN_BRI_MINUS);
    add_btn(BAR_X + BAR_W + 8, BRI_BAR_Y - 6, 36, 36, BTN_BRI_PLUS);
    add_btn(BAR_X, VOL_BAR_Y, BAR_W, BAR_H, BTN_SOUND_TOGGLE);
    add_btn(UI_PAD, 214, UI_CONTENT_W, 44, BTN_CALIB);
    add_btn(UI_PAD, 266, UI_CONTENT_W, 44, BTN_BACK);
}

static void draw_level_bar(int bar_y, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    const int fill_w = BAR_W * pct / 100;
    tft.fillRoundRect(BAR_X, bar_y, BAR_W, BAR_H, 6, TH->card);
    tft.drawRoundRect(BAR_X, bar_y, BAR_W, BAR_H, 6, TH->border);
    if (fill_w > 4)
        tft.fillRoundRect(BAR_X + 2, bar_y + 2, fill_w - 4, BAR_H - 4, 4, TH->accent);

    tft.fillRect(BAR_X, bar_y + BAR_H + 4, BAR_W, 18, TH->bg);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(buf, SCREEN_CX, bar_y + BAR_H + 16, 2);
}

static void draw_brightness_bar() {
    draw_level_bar(BRI_BAR_Y, display_brightness_percent());
}

static void draw_sound_toggle() {
    const bool on = buzzer_sound_on();
    const uint16_t bg = on ? TH->accent : TH->card;
    tft.fillRoundRect(BAR_X, VOL_BAR_Y, BAR_W, BAR_H, 6, bg);
    tft.drawRoundRect(BAR_X, VOL_BAR_Y, BAR_W, BAR_H, 6, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(on ? TH->bg : TH->text_hi, bg);
    tft.drawString(on ? "Ligado" : "Mutado", SCREEN_CX, VOL_BAR_Y + BAR_H / 2, 2);
}

static void draw_screen() {
    tft.fillScreen(TH->bg);
    ui_draw_app_header(UI_ICON_SLIDERS, "Ajustes", "Som e display");

    ui_icon_draw(UI_PAD, 64, 24, UI_ICON_SUN, TH->accent);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString("Brilho", UI_PAD + 32, 70, 2);

    ui_icon_draw(UI_PAD, 124, 24, UI_ICON_PLAY, TH->accent);
    tft.drawString("Som", UI_PAD + 32, 130, 2);

    for (int i = 0; i < s_btn_count; i++) {
        const SettingsBtn* b = &s_btns[i];
        const uint16_t bg = TH->card;
        tft.fillRoundRect(b->x, b->y, b->w, b->h, UI_CARD_R, bg);
        tft.drawRoundRect(b->x, b->y, b->w, b->h, UI_CARD_R, TH->border);
        tft.setTextDatum(MC_DATUM);
        if (b->id == BTN_BRI_MINUS) {
            tft.setTextColor(TH->text_hi, bg);
            tft.drawString("-", b->x + b->w / 2, b->y + b->h / 2, 4);
        } else if (b->id == BTN_BRI_PLUS) {
            tft.setTextColor(TH->text_hi, bg);
            tft.drawString("+", b->x + b->w / 2, b->y + b->h / 2, 4);
        } else if (b->id == BTN_SOUND_TOGGLE) {
            continue;
        } else if (b->id == BTN_CALIB) {
            ui_icon_draw(b->x + 10, b->y + 10, 24, UI_ICON_TARGET, TH->icon);
            tft.setTextColor(TH->text_hi, bg);
            tft.drawString("Calibrar toque", b->x + b->w / 2 + 8, b->y + b->h / 2, 2);
        } else if (b->id == BTN_BACK) {
            tft.setTextColor(TH->text_mute, bg);
            tft.drawString("Voltar", b->x + b->w / 2, b->y + b->h / 2, 2);
        }
    }

    draw_brightness_bar();
    draw_sound_toggle();
}

static int hit_btn(int16_t tx, int16_t ty) {
    for (int i = 0; i < s_btn_count; i++) {
        const SettingsBtn* b = &s_btns[i];
        if (tx >= b->x && tx < b->x + b->w && ty >= b->y && ty < b->y + b->h)
            return b->id;
    }
    return 0;
}

int ui_settings_show() {
    build_layout();
    buzzer_init();
    draw_screen();
    touch_wait_release();

    bool was_pressed = false;
    while (true) {
        touch_poll();
        const bool down = touch_is_pressed();
        if (down && !was_pressed) {
            const int id = hit_btn(touch_get_x(), touch_get_y());
            if (id == BTN_BRI_MINUS) {
                display_brightness_step(-1);
                draw_brightness_bar();
            } else if (id == BTN_BRI_PLUS) {
                display_brightness_step(1);
                draw_brightness_bar();
            } else if (id == BTN_SOUND_TOGGLE) {
                buzzer_sound_toggle();
                draw_sound_toggle();
                if (buzzer_sound_on())
                    buzzer_play(SFX_TICK);
            } else if (id == BTN_CALIB) {
                touch_wait_release();
                return UI_SETTINGS_CALIBRATE;
            } else if (id == BTN_BACK) {
                touch_wait_release();
                return UI_SETTINGS_BACK;
            }
        }
        was_pressed = down;
        delay(10);
    }
}
