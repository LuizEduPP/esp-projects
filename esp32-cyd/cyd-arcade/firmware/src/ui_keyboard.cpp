#include "ui_keyboard.h"
#include "display.h"
#include "hw_config.h"
#include "touch_input.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define TH ui_theme_get()
#define KB_ROW_H  26
#define KB_GAP    2
#define KB_TOP    (PLAY_Y + 76)
#define NAME_Y    (PLAY_Y + 44)
#define NAME_H    26

enum KeyId { KEY_CHAR = 0, KEY_BS, KEY_OK };

struct KeyBtn {
    int x, y, w, h;
    KeyId id;
    char ch;
};

static KeyBtn s_keys[32];
static int s_key_count;

static void add_key(int x, int y, int w, int h, KeyId id, char ch) {
    if (s_key_count >= (int)(sizeof(s_keys) / sizeof(s_keys[0]))) return;
    s_keys[s_key_count++] = {x, y, w, h, id, ch};
}

static void build_layout() {
    s_key_count = 0;
    const char* row0 = "QWERTYUIOP";
    const char* row1 = "ASDFGHJKL";
    const char* row2 = "ZXCVBNM";

    int y = KB_TOP;
    for (int i = 0; row0[i]; i++) {
        const int n = (int)strlen(row0);
        const int kw = (SCREEN_W - 8 - (n - 1) * KB_GAP) / n;
        add_key(4 + i * (kw + KB_GAP), y, kw, KB_ROW_H, KEY_CHAR, row0[i]);
    }

    y += KB_ROW_H + KB_GAP;
    const int n1 = (int)strlen(row1);
    const int kw1 = (SCREEN_W - 8 - (n1 - 1) * KB_GAP) / n1;
    const int off1 = (SCREEN_W - (n1 * kw1 + (n1 - 1) * KB_GAP)) / 2;
    for (int i = 0; row1[i]; i++)
        add_key(off1 + i * (kw1 + KB_GAP), y, kw1, KB_ROW_H, KEY_CHAR, row1[i]);

    y += KB_ROW_H + KB_GAP;
    const int n2 = (int)strlen(row2);
    const int kw2 = 20;
    const int bs_w = 36;
    const int row2_w = n2 * kw2 + (n2 - 1) * KB_GAP + KB_GAP + bs_w;
    int x = (SCREEN_W - row2_w) / 2;
    for (int i = 0; row2[i]; i++) {
        add_key(x, y, kw2, KB_ROW_H, KEY_CHAR, row2[i]);
        x += kw2 + KB_GAP;
    }
    add_key(x, y, bs_w, KB_ROW_H, KEY_BS, 0);

    y += KB_ROW_H + KB_GAP + 4;
    add_key(16, y, SCREEN_W - 32, 32, KEY_OK, 0);
}

static void draw_key(int i, bool sel) {
    const KeyBtn* k = &s_keys[i];
    const uint16_t bg = sel ? TH->accent : TH->card;
    const uint16_t fg = sel ? TH->text_hi : TH->text_mute;
    tft.fillRoundRect(k->x, k->y, k->w, k->h, 4, bg);
    tft.drawRoundRect(k->x, k->y, k->w, k->h, 4, sel ? TH->accent_hi : TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    if (k->id == KEY_BS)
        tft.drawString("<", k->x + k->w / 2, k->y + k->h / 2, 2);
    else if (k->id == KEY_OK)
        tft.drawString("OK", k->x + k->w / 2, k->y + k->h / 2, 2);
    else {
        char s[2] = {k->ch, 0};
        tft.drawString(s, k->x + k->w / 2, k->y + k->h / 2, 2);
    }
}

static void draw_keys(int highlight) {
    for (int i = 0; i < s_key_count; i++)
        draw_key(i, i == highlight);
}

static void draw_header(const char* title, int score) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TH->accent_hi, TH->play_field);
    tft.drawString(title, SCREEN_CX, PLAY_Y + 12, 2);
    char buf[24];
    snprintf(buf, sizeof(buf), "Pontos: %d", score);
    tft.setTextColor(TH->text_hi, TH->play_field);
    tft.drawString(buf, SCREEN_CX, PLAY_Y + 32, 1);
}

static void draw_name_field(const char* name) {
    tft.fillRoundRect(12, NAME_Y, SCREEN_W - 24, NAME_H, 4, TH->card);
    tft.drawRoundRect(12, NAME_Y, SCREEN_W - 24, NAME_H, 4, TH->border);
    char disp[SCORE_NAME_LEN + 2];
    snprintf(disp, sizeof(disp), "%s", name);
    const int n = (int)strlen(disp);
    if (n < SCORE_NAME_LEN) {
        disp[n] = '|';
        disp[n + 1] = '\0';
    }
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString(disp, 20, NAME_Y + NAME_H / 2, 2);
}

static void draw_screen_init(const char* title, int score, const char* name) {
    tft.fillRect(0, PLAY_Y, SCREEN_W, PLAY_H, TH->play_field);
    draw_header(title, score);
    draw_name_field(name);
    draw_keys(-1);
}

static int hit_key(int16_t tx, int16_t ty) {
    for (int i = 0; i < s_key_count; i++) {
        const KeyBtn* k = &s_keys[i];
        if (tx >= k->x && tx < k->x + k->w && ty >= k->y && ty < k->y + k->h)
            return i;
    }
    return -1;
}

bool ui_keyboard_enter_name(const char* title, int score, char* out, int out_len) {
    if (!out || out_len <= 0) return false;
    out[0] = '\0';

    build_layout();
    draw_screen_init(title, score, out);
    touch_wait_release();

    int pending = -1;
    int last_highlight = -1;
    bool was = false;

    for (;;) {
        touch_poll();
        const bool down = touch_is_pressed();
        if (down) {
            const int hit = hit_key(touch_get_x(), touch_get_y());
            if (hit >= 0 && hit != pending) {
                if (last_highlight >= 0 && last_highlight != hit)
                    draw_key(last_highlight, false);
                pending = hit;
                draw_key(hit, true);
                last_highlight = hit;
            }
        } else if (was && pending >= 0) {
            const KeyBtn* k = &s_keys[pending];
            if (k->id == KEY_OK) {
                if (out[0] == '\0') strncpy(out, "PLAYER", out_len - 1);
                out[out_len - 1] = '\0';
                touch_wait_release();
                return true;
            }
            if (k->id == KEY_BS) {
                const int n = (int)strlen(out);
                if (n > 0) out[n - 1] = '\0';
            } else if (k->id == KEY_CHAR && strlen(out) < (size_t)(out_len - 1)) {
                const int n = (int)strlen(out);
                out[n] = k->ch;
                out[n + 1] = '\0';
            }
            draw_key(pending, false);
            last_highlight = -1;
            draw_name_field(out);
            pending = -1;
        }
        was = down;
        delay(10);
    }
}
