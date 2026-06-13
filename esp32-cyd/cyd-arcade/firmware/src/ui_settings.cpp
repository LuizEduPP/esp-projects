#include "ui_settings.h"
#include "display.h"
#include "hw_config.h"
#include "touch_input.h"
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
    BTN_MINUS = 1,
    BTN_PLUS,
    BTN_CALIB,
    BTN_BACK,
};

static SettingsBtn s_btns[4];
static int s_btn_count;

static void add_btn(int x, int y, int w, int h, int id) {
    if (s_btn_count >= (int)(sizeof(s_btns) / sizeof(s_btns[0]))) return;
    s_btns[s_btn_count++] = {x, y, w, h, id};
}

static void build_layout() {
    s_btn_count = 0;
    const int bar_x = UI_PAD + 44;
    const int bar_w = SCREEN_W - UI_PAD * 2 - 88;
    const int bar_y = 108;
    add_btn(UI_PAD, bar_y - 6, 36, 36, BTN_MINUS);
    add_btn(bar_x + bar_w + 8, bar_y - 6, 36, 36, BTN_PLUS);
    add_btn(UI_PAD, 168, UI_CONTENT_W, 44, BTN_CALIB);
    add_btn(UI_PAD, 228, UI_CONTENT_W, 44, BTN_BACK);
}

static void draw_brightness_bar() {
    const int bar_x = UI_PAD + 44;
    const int bar_w = SCREEN_W - UI_PAD * 2 - 88;
    const int bar_y = 108;
    const int bar_h = 24;
    const int pct = display_brightness_percent();
    const int fill_w = bar_w * pct / 100;

    tft.fillRoundRect(bar_x, bar_y, bar_w, bar_h, 6, TH->card);
    tft.drawRoundRect(bar_x, bar_y, bar_w, bar_h, 6, TH->border);
    if (fill_w > 4)
        tft.fillRoundRect(bar_x + 2, bar_y + 2, fill_w - 4, bar_h - 4, 4, TH->accent);

    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", pct);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(buf, SCREEN_CX, bar_y + bar_h + 18, 2);
}

static void draw_screen() {
    tft.fillScreen(TH->bg);
    tft.drawFastHLine(0, UI_HDR_H - 1, SCREEN_W, TH->border);

    ui_icon_draw(UI_PAD, 10, 24, UI_ICON_SLIDERS, TH->icon);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString("Ajustes", UI_PAD + 32, 14, 2);

    ui_icon_draw(UI_PAD, 72, 24, UI_ICON_SUN, TH->accent);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString("Brilho", UI_PAD + 32, 78, 2);

    for (int i = 0; i < s_btn_count; i++) {
        const SettingsBtn* b = &s_btns[i];
        const uint16_t bg = TH->card;
        tft.fillRoundRect(b->x, b->y, b->w, b->h, 8, bg);
        tft.drawRoundRect(b->x, b->y, b->w, b->h, 8, TH->border);
        tft.setTextDatum(MC_DATUM);
        if (b->id == BTN_MINUS) {
            tft.setTextColor(TH->text_hi, bg);
            tft.drawString("-", b->x + b->w / 2, b->y + b->h / 2, 4);
        } else if (b->id == BTN_PLUS) {
            tft.setTextColor(TH->text_hi, bg);
            tft.drawString("+", b->x + b->w / 2, b->y + b->h / 2, 4);
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
    draw_screen();
    touch_wait_release();

    bool was_pressed = false;
    while (true) {
        touch_poll();
        const bool down = touch_is_pressed();
        if (down && !was_pressed) {
            const int id = hit_btn(touch_get_x(), touch_get_y());
            if (id == BTN_MINUS) {
                display_brightness_step(-1);
                draw_brightness_bar();
            } else if (id == BTN_PLUS) {
                display_brightness_step(1);
                draw_brightness_bar();
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
