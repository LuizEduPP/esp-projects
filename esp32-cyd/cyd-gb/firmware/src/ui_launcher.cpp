#include "ui_launcher.h"
#include "display.h"
#include "touch_input.h"
#include "emulator_bridge.h"
#include "hw_config.h"
#include "i18n.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <math.h>

#define HDR_H      36
#define FTR_H      44
#define FTR_Y      (SCREEN_H - FTR_H)
#define LIST_TOP   HDR_H
#define LIST_BOT   FTR_Y
#define ITEMS_PP   5
#define ITEM_H     42
#define ITEM_X     8
#define ITEM_W     (SCREEN_W - ITEM_X * 2)
#define GEAR_X     (SCREEN_W - 24)
#define GEAR_Y     (HDR_H / 2)
#define GEAR_R     16

#define TH ui_theme_get()

static void wait_release() {
    while (touch_is_pressed()) delay(10);
    delay(80);
}

static void draw_gear(int cx, int cy) {
    tft.fillCircle(cx, cy, GEAR_R, TH->border);
    tft.drawCircle(cx, cy, GEAR_R, TH->accent);
    tft.drawCircle(cx, cy, 5, TH->mute);
    for (int i = 0; i < 6; i++) {
        float a = i * 3.14159f / 3.0f;
        int x1 = cx + (int)(7 * cos(a));
        int y1 = cy + (int)(7 * sin(a));
        int x2 = cx + (int)(12 * cos(a));
        int y2 = cy + (int)(12 * sin(a));
        tft.drawLine(x1, y1, x2, y2, TH->mute);
    }
}

static void draw_header(int cnt) {
    tft.fillRect(0, 0, SCREEN_W, HDR_H, TH->surface);
    tft.drawFastHLine(0, HDR_H - 1, SCREEN_W, TH->accent);

    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(tr(STR_GAMES), 12, 13, 2);

    char sub[20];
    snprintf(sub, sizeof(sub), tr(STR_ROMS_FMT), cnt);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString(sub, 12, 28, 1);

    draw_gear(GEAR_X, GEAR_Y);
}

static void draw_rom_card(RomEntry* r, int y, bool sel) {
    uint16_t bg = sel ? TH->card_sel : TH->card;
    uint16_t bd = sel ? TH->accent : TH->border;
    tft.fillRoundRect(ITEM_X, y, ITEM_W, ITEM_H - 4, 8, bg);
    if (sel) tft.drawRoundRect(ITEM_X, y, ITEM_W, ITEM_H - 4, 8, bd);

    uint16_t bc = r->is_gbc ? TH->accent : TH->mute;
    tft.fillRoundRect(ITEM_X + 8, y + 8, 28, 22, 5, bc);
    tft.setTextColor(TH->text_lo, bc);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(r->is_gbc ? "C" : "D", ITEM_X + 22, y + 19, 2);

    char nm[28];
    strncpy(nm, r->filename, 26);
    nm[26] = 0;
    char* dot = strrchr(nm, '.');
    if (dot) *dot = 0;
    tft.setTextColor(sel ? TH->text_hi : TH->text_hi, bg);
    tft.setTextDatum(ML_DATUM);
    tft.drawString(nm, ITEM_X + 44, y + 14, 2);

    char sz[12];
    snprintf(sz, sizeof(sz), "%uK", r->size / 1024);
    tft.setTextColor(TH->text_mute, bg);
    tft.drawString(sz, ITEM_X + 44, y + 30, 1);

    if (sel) tft.fillCircle(ITEM_X + ITEM_W - 12, y + ITEM_H / 2 - 2, 4, TH->accent);
}

static void draw_footer(int pg, int tp) {
    tft.fillRect(0, FTR_Y, SCREEN_W, FTR_H, TH->surface);
    tft.drawFastHLine(0, FTR_Y, SCREEN_W, TH->border);

    tft.fillRoundRect(10, FTR_Y + 8, 72, 28, 8, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->accent, TH->border);
    tft.drawString(tr(STR_PREV), 46, FTR_Y + 22, 2);

    if (tp > 1) {
        char ps[12];
        snprintf(ps, sizeof(ps), "%d / %d", pg + 1, tp);
        tft.setTextColor(TH->text_mute, TH->surface);
        tft.drawString(ps, SCREEN_CX, FTR_Y + 22, 2);
    }

    tft.fillRoundRect(SCREEN_W - 82, FTR_Y + 8, 72, 28, 8, TH->border);
    tft.setTextColor(TH->accent, TH->border);
    tft.drawString(tr(STR_NEXT), SCREEN_W - 46, FTR_Y + 22, 2);
}

static void draw_list(RomEntry* r, int cnt, int pg, int sel) {
    tft.fillRect(0, LIST_TOP, SCREEN_W, LIST_BOT - LIST_TOP, TH->bg);
    int s = pg * ITEMS_PP;
    int e = min(s + ITEMS_PP, cnt);
    for (int i = s; i < e; i++) {
        int y = LIST_TOP + 4 + (i - s) * ITEM_H;
        draw_rom_card(&r[i], y, i == sel);
    }
    draw_footer(pg, (cnt + ITEMS_PP - 1) / ITEMS_PP);
}

static bool hit_gear(int16_t tx, int16_t ty) {
    int dx = tx - GEAR_X, dy = ty - GEAR_Y;
    return dx * dx + dy * dy <= (GEAR_R + 4) * (GEAR_R + 4);
}

int launcher_show(RomEntry* roms, int cnt) {
    int pg = 0;
    tft.fillScreen(TH->bg);
    draw_header(cnt);

    if (cnt == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->menu_danger);
        tft.drawString(tr(STR_NO_ROMS), SCREEN_CX, 120, 4);
        tft.setTextColor(TH->text_mute);
        tft.drawString(tr(STR_NO_ROMS_HINT), SCREEN_CX, 150, 2);
        while (true) delay(1000);
    }

    draw_list(roms, cnt, pg, -1);
    wait_release();

    bool was_pressed = false;
    bool cfg_pending = false;
    int pending_sel = -1;
    int16_t last_tx = 0, last_ty = 0;

    while (true) {
        bool pressed = touch_is_pressed();
        if (pressed) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            last_tx = tx;
            last_ty = ty;
            if (hit_gear(tx, ty)) cfg_pending = true;
            else if (ty >= LIST_TOP && ty < LIST_BOT) {
                int idx = pg * ITEMS_PP + (ty - LIST_TOP - 4) / ITEM_H;
                if (idx >= pg * ITEMS_PP && idx < min(pg * ITEMS_PP + ITEMS_PP, cnt) && idx != pending_sel) {
                    pending_sel = idx;
                    draw_list(roms, cnt, pg, pending_sel);
                }
            }
        } else if (was_pressed) {
            if (cfg_pending) return -3;
            if (pending_sel >= 0) return pending_sel;
            if (last_ty >= FTR_Y) {
                int tp = (cnt + ITEMS_PP - 1) / ITEMS_PP;
                if (last_tx < 90 && pg > 0) {
                    pg--;
                    draw_list(roms, cnt, pg, -1);
                } else if (last_tx > SCREEN_W - 90 && pg < tp - 1) {
                    pg++;
                    draw_list(roms, cnt, pg, -1);
                }
            }
            cfg_pending = false;
            pending_sel = -1;
        }
        was_pressed = pressed;
        delay(10);
    }
}

static void draw_menu_btn(int y, int h, const char* t, uint16_t fg, uint16_t bg, bool hl) {
    int bx = 10;
    int bw = SCREEN_W - 20;
    tft.fillRoundRect(bx, y, bw, h, 8, hl ? TH->border : bg);
    if (hl) tft.drawRoundRect(bx, y, bw, h, 8, TH->accent);
    tft.setTextColor(fg, hl ? TH->border : bg);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(t, SCREEN_CX, y + h / 2, 2);
}

int launcher_ingame_menu() {
    tft.fillScreen(TH->bg);

    tft.fillRect(0, 0, SCREEN_W, 36, TH->surface);
    tft.drawFastHLine(0, 35, SCREEN_W, TH->accent);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.setTextDatum(MC_DATUM);
    tft.drawString(tr(STR_PAUSED), SCREEN_CX, 18, 2);

    #define MI 6
    const int MBH = 36;
    const int MBG = 8;
    const int MY0 = 48;
    const StringId lb_ids[MI] = {
        STR_MENU_RESUME, STR_MENU_SAVE, STR_MENU_LOAD,
        STR_MENU_SETTINGS, STR_MENU_CALIBRATE, STR_MENU_QUIT
    };
    uint16_t fc[MI] = {TH->menu_primary, TH->accent, TH->accent, TH->text_hi, TH->text_hi, TH->menu_danger};
    uint16_t bg[MI] = {TH->card_sel, TH->card, TH->card, TH->card, TH->card, TH->card};

    for (int i = 0; i < MI; i++)
        draw_menu_btn(MY0 + i * (MBH + MBG), MBH, tr(lb_ids[i]), fc[i], bg[i], i == 0);

    wait_release();

    int hl = -1;
    while (true) {
        if (touch_is_pressed()) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            for (int i = 0; i < MI; i++) {
                int y = MY0 + i * (MBH + MBG);
                if (tx >= 10 && tx <= SCREEN_W - 10 && ty >= y && ty < y + MBH) {
                    if (hl != i) {
                        if (hl >= 0)
                            draw_menu_btn(MY0 + hl * (MBH + MBG), MBH, tr(lb_ids[hl]), fc[hl], bg[hl], hl == 0);
                        draw_menu_btn(y, MBH, tr(lb_ids[i]), fc[i], bg[i], true);
                        hl = i;
                    }
                    break;
                }
            }
        } else if (hl >= 0) {
            int s = hl;
            hl = -1;
            switch (s) {
                case 0: return 0;
                case 1: return 1;
                case 2: return 2;
                case 3: return 5;
                case 4: return 4;
                case 5: return 3;
            }
        }
        delay(15);
    }
}

void launcher_settings_menu() {
    uint8_t pal = emu_get_palette();
    uint8_t fs = emu_get_frame_skip();
    uint8_t bl = display_get_backlight();
    uint8_t lang = i18n_get_lang();

    const int HDR = 32;
    const int RX = 8;
    const int RW = SCREEN_W - 16;
    const int BOX_H = 28;
    const int ROW_H = 48;
    const int Y0 = 38;
    const int OK_Y = 278;
    const int OK_H = 34;

    static const StringId row_labels[4] = {
        STR_PALETTE, STR_FRAME_SKIP, STR_BRIGHTNESS, STR_LANGUAGE
    };

    auto row_y = [&](int i) { return Y0 + i * ROW_H; };
    auto box_y = [&](int i) { return row_y(i) + 12; };

    auto draw_row = [&](int i, const char* value, bool swatches) {
        int by = box_y(i);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(TH->text_mute, TH->bg);
        tft.drawString(tr(row_labels[i]), RX, row_y(i), 1);
        tft.fillRoundRect(RX, by, RW, BOX_H, 6, TH->surface);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->text_hi, TH->surface);
        tft.drawString(value, SCREEN_CX, by + BOX_H / 2 - (swatches ? 3 : 0), 1);
        tft.setTextColor(TH->accent, TH->surface);
        tft.drawString("<", RX + 14, by + BOX_H / 2, 2);
        tft.drawString(">", RX + RW - 14, by + BOX_H / 2, 2);
        if (swatches) {
            for (int c = 0; c < 4; c++)
                tft.fillRoundRect(RX + 36 + c * 38, by + BOX_H - 9, 32, 5, 1, emu_palette_color(pal, c));
        }
    };

    auto draw_settings = [&]() {
        tft.fillScreen(TH->bg);
        tft.fillRect(0, 0, SCREEN_W, HDR, TH->surface);
        tft.drawFastHLine(0, HDR - 1, SCREEN_W, TH->accent);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->text_hi, TH->surface);
        tft.drawString(tr(STR_SETTINGS), SCREEN_CX, HDR / 2, 2);

        char palstr[24];
        snprintf(palstr, sizeof(palstr), "#%d %s", pal + 1, emu_get_palette_name(pal));
        draw_row(0, palstr, true);

        char fss[20];
        snprintf(fss, sizeof(fss), "%d (~%dfps)", fs, fs == 0 ? 60 : 60 / (fs + 1));
        draw_row(1, fss, false);

        char bls[8];
        snprintf(bls, sizeof(bls), "%d%%", bl * 100 / 255);
        draw_row(2, bls, false);

        draw_row(3, i18n_lang_label(lang), false);

        tft.fillRoundRect(RX, OK_Y, RW, OK_H, 8, TH->accent);
        tft.setTextColor(TH->text_lo, TH->accent);
        tft.drawString(tr(STR_SAVE_BACK), SCREEN_CX, OK_Y + OK_H / 2, 2);
    };

    auto hit_row = [&](int i, int16_t tx, int16_t ty) {
        int by = box_y(i);
        return ty >= by && ty < by + BOX_H && tx >= RX && tx <= RX + RW;
    };

    draw_settings();
    wait_release();

    while (true) {
        if (touch_is_pressed()) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            bool changed = false;

            if (hit_row(0, tx, ty)) {
                if (tx < SCREEN_CX) pal = (pal + NUM_PALETTES - 1) % NUM_PALETTES;
                else if (tx > SCREEN_CX) pal = (pal + 1) % NUM_PALETTES;
                changed = true;
            } else if (hit_row(1, tx, ty)) {
                if (tx < SCREEN_CX && fs > 0) fs--;
                else if (tx > SCREEN_CX && fs < 4) fs++;
                changed = true;
            } else if (hit_row(2, tx, ty)) {
                if (tx < SCREEN_CX && bl > 30) bl -= 25;
                else if (tx > SCREEN_CX && bl < 255) bl = min(255, bl + 25);
                changed = true;
            } else if (hit_row(3, tx, ty)) {
                if (tx < SCREEN_CX) lang = (lang + LANG_COUNT - 1) % LANG_COUNT;
                else if (tx > SCREEN_CX) lang = (lang + 1) % LANG_COUNT;
                i18n_set_lang(lang);
                changed = true;
            } else if (ty >= OK_Y && ty < OK_Y + OK_H && tx >= RX && tx <= RX + RW) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                i18n_set_lang(lang);
                touch_save_settings(pal, fs, bl, lang);
                wait_release();
                return;
            }

            if (changed) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                draw_settings();
                delay(120);
            }
        }
        delay(20);
    }
}
