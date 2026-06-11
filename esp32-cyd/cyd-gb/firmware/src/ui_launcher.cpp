#include "ui_launcher.h"
#include "display.h"
#include "touch_input.h"
#include "emulator_bridge.h"
#include "hw_config.h"
#include <Arduino.h>

#define ITEMS_PP 7
#define ITEM_H   34
#define ITEM_Y0  44
#define ITEM_X   8
#define ITEM_W   (SCREEN_W - ITEM_X * 2)

static void wait_release() {
    while (touch_is_pressed()) delay(10);
    delay(100);
}

static void draw_header(const char* t) {
    tft.fillRect(0, 0, SCREEN_W, 36, 0x18C3);
    tft.setTextColor(TFT_WHITE, 0x18C3); tft.setTextDatum(ML_DATUM);
    tft.drawString(t, 10, 18, 2);
    tft.setTextDatum(MR_DATUM); tft.setTextColor(0x7BEF, 0x18C3);
    tft.drawString("CYD-GB", SCREEN_W - 10, 18, 1);
}

static void draw_list(RomEntry* r, int cnt, int pg, int sel) {
    int s = pg * ITEMS_PP, e = min(s + ITEMS_PP, cnt);
    tft.fillRect(0, 38, SCREEN_W, SCREEN_H - 58, TFT_BLACK);

    for (int i = s; i < e; i++) {
        int y = ITEM_Y0 + (i - s) * ITEM_H;
        uint16_t bg = (i == sel) ? 0x0014 : 0x0000;
        uint16_t fg = (i == sel) ? 0xFFE0 : TFT_WHITE;
        tft.fillRoundRect(ITEM_X, y, ITEM_W, ITEM_H - 4, 4, bg);

        uint16_t bc = r[i].is_gbc ? 0x07E0 : 0x7BEF;
        const char* bt = r[i].is_gbc ? "GBC" : "GB";
        tft.fillRoundRect(ITEM_X + 3, y + 5, 26, 18, 3, bc);
        tft.setTextColor(TFT_BLACK, bc); tft.setTextDatum(MC_DATUM);
        tft.drawString(bt, ITEM_X + 16, y + 14, 1);

        char nm[30]; strncpy(nm, r[i].filename, 28); nm[28] = 0;
        char* dot = strrchr(nm, '.'); if (dot) *dot = 0;
        tft.setTextColor(fg, bg); tft.setTextDatum(ML_DATUM);
        tft.drawString(nm, ITEM_X + 34, y + ITEM_H / 2 - 2, 2);

        char sz[12]; snprintf(sz, 12, "%uK", r[i].size / 1024);
        tft.setTextColor(0x7BEF, bg); tft.setTextDatum(MR_DATUM);
        tft.drawString(sz, SCREEN_W - 8, y + ITEM_H / 2 - 2, 1);
    }

    tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 20, 0x18C3);
    int tp = (cnt + ITEMS_PP - 1) / ITEMS_PP;
    if (tp > 1) {
        tft.setTextColor(TFT_WHITE, 0x18C3); tft.setTextDatum(MC_DATUM);
        char ps[16]; snprintf(ps, 16, "< %d/%d >", pg + 1, tp);
        tft.drawString(ps, SCREEN_CX, SCREEN_H - 10, 1);
    }
    tft.setTextColor(0xFFE0, 0x18C3); tft.setTextDatum(ML_DATUM);
    tft.drawString("[CFG]", 5, SCREEN_H - 10, 1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("[CAL]", SCREEN_W - 5, SCREEN_H - 10, 1);
}

int launcher_show(RomEntry* roms, int cnt) {
    int pg = 0;
    tft.fillScreen(TFT_BLACK);
    draw_header("Game Boy ROMs");

    if (cnt == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.drawString("No ROMs found!", SCREEN_CX, 100, 4);
        tft.setTextColor(0x7BEF); tft.drawString("Put .gb in /roms/gb/", SCREEN_CX, 140, 2);
        tft.drawString("on your SD card", SCREEN_CX, 165, 2);
        while (true) delay(1000);
    }

    draw_list(roms, cnt, pg, -1);
    wait_release();

    uint32_t dbg_t = 0;
    bool was_pressed = false;
    bool cal_pending = false;
    bool cfg_pending = false;
    int pending_sel = -1;

    while (true) {
        bool pressed = touch_is_pressed();
        if (pressed) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            if (ty >= ITEM_Y0 && ty < ITEM_Y0 + ITEMS_PP * ITEM_H) {
                int idx = pg * ITEMS_PP + (ty - ITEM_Y0) / ITEM_H;
                if (idx < cnt && idx != pending_sel) {
                    pending_sel = idx;
                    draw_list(roms, cnt, pg, pending_sel);
                }
            }
            if (ty >= SCREEN_H - 20) {
                int tp = (cnt + ITEMS_PP - 1) / ITEMS_PP;
                if (tx < 60 && pg > 0) { pg--; pending_sel = -1; draw_list(roms, cnt, pg, -1); delay(300); }
                else if (tx > SCREEN_W - 58 && tx < SCREEN_W - 2) cal_pending = true;
                else if (tx < 58) cfg_pending = true;
                else if (tx > SCREEN_W - 140 && tx < SCREEN_W - 60 && pg < tp - 1) {
                    pg++; pending_sel = -1; draw_list(roms, cnt, pg, -1); delay(300);
                }
            }
        } else {
            if (was_pressed) {
                if (cal_pending) return -2;
                if (cfg_pending) return -3;
                if (pending_sel >= 0) return pending_sel;
            }
            cal_pending = false;
            cfg_pending = false;
            pending_sel = -1;
        }
        was_pressed = pressed;

        if (millis() - dbg_t > 3000) {
            dbg_t = millis();
            Serial.printf("[LAUNCH] pg=%d pending=%d touch=%d,%d pressed=%d\n",
                          pg, pending_sel, touch_get_x(), touch_get_y(), (int)pressed);
        }
        delay(10);
    }
}

static void mbtn(int y, const char* t, uint16_t fg, bool hl) {
    int bx = (SCREEN_W - 190) / 2;
    uint16_t bg = hl ? 0x2945 : 0x1082;
    tft.fillRoundRect(bx, y, 190, 26, 5, bg);
    tft.drawRoundRect(bx, y, 190, 26, 5, 0x528A);
    tft.setTextColor(fg, bg); tft.setTextDatum(MC_DATUM);
    tft.drawString(t, SCREEN_CX, y + 13, 2);
}

int launcher_ingame_menu() {
    int bx = (SCREEN_W - 200) / 2;
    int by = 20;
    int bh = SCREEN_H - 40;
    tft.fillRect(bx, by, 200, bh, TFT_BLACK);
    tft.drawRoundRect(bx, by, 200, bh, 6, 0x528A);
    tft.setTextColor(0xFFE0, TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.drawString("PAUSED", SCREEN_CX, by + 18, 4);

    #define MI 6
    int yp[MI] = {by + 38, by + 78, by + 118, by + 158, by + 198, by + 238};
    const char* lb[MI] = {"Continuar", "Salvar", "Carregar", "Config", "Calibrar", "Voltar ao Menu"};
    uint16_t fc[MI] = {TFT_GREEN, 0x07FF, 0x07FF, 0xFFE0, 0xFFE0, TFT_RED};
    for (int i = 0; i < MI; i++) mbtn(yp[i], lb[i], fc[i], false);
    wait_release();

    int hl = -1;
    while (true) {
        if (touch_is_pressed()) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            if (tx >= bx && tx <= bx + 200) {
                for (int i = 0; i < MI; i++) if (ty >= yp[i] && ty < yp[i] + 26) {
                    if (hl != i) {
                        if (hl >= 0) mbtn(yp[hl], lb[hl], fc[hl], false);
                        mbtn(yp[i], lb[i], fc[i], true);
                        hl = i;
                    }
                    break;
                }
            }
        } else if (hl >= 0) {
            int s = hl; hl = -1;
            switch (s) { case 0: return 0; case 1: return 1; case 2: return 2; case 3: return 5; case 4: return 4; case 5: return 3; }
        }
        delay(15);
    }
}

void launcher_settings_menu() {
    uint8_t pal = emu_get_palette();
    uint8_t fs = emu_get_frame_skip();
    uint8_t bl = display_get_backlight();
    int row_w = SCREEN_W - 20;
    int row_x = 10;

    auto draw_settings = [&]() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(0xFFE0); tft.drawString("SETTINGS", SCREEN_CX, 18, 4);

        tft.setTextColor(TFT_WHITE); tft.drawString("Color Palette:", SCREEN_CX, 48, 2);
        tft.fillRoundRect(row_x, 62, row_w, 28, 5, 0x1082);
        char palstr[40]; snprintf(palstr, 40, "%d/%d %s", pal + 1, NUM_PALETTES, emu_get_palette_name(pal));
        tft.setTextColor(0x07E0, 0x1082); tft.drawString(palstr, SCREEN_CX, 76, 2);
        tft.setTextColor(0x7BEF, 0x1082);
        tft.setTextDatum(ML_DATUM); tft.drawString("<<", row_x + 8, 76, 2);
        tft.setTextDatum(MR_DATUM); tft.drawString(">>", SCREEN_W - 8, 76, 2);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE); tft.drawString("Frame Skip:", SCREEN_CX, 108, 2);
        tft.fillRoundRect(row_x, 122, row_w, 28, 5, 0x1082);
        char fss[16]; snprintf(fss, 16, "%d (FPS ~%d)", fs, fs == 0 ? 60 : 60 / (fs + 1));
        tft.setTextColor(0x07E0, 0x1082); tft.drawString(fss, SCREEN_CX, 136, 2);
        tft.setTextColor(0x7BEF, 0x1082);
        tft.setTextDatum(ML_DATUM); tft.drawString("<", row_x + 8, 136, 2);
        tft.setTextDatum(MR_DATUM); tft.drawString(">", SCREEN_W - 8, 136, 2);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE); tft.drawString("Brightness:", SCREEN_CX, 168, 2);
        tft.fillRoundRect(row_x, 182, row_w, 28, 5, 0x1082);
        char bls[16]; snprintf(bls, 16, "%d%%", bl * 100 / 255);
        tft.setTextColor(0x07E0, 0x1082); tft.drawString(bls, SCREEN_CX, 196, 2);
        tft.setTextColor(0x7BEF, 0x1082);
        tft.setTextDatum(ML_DATUM); tft.drawString("<", row_x + 8, 196, 2);
        tft.setTextDatum(MR_DATUM); tft.drawString(">", SCREEN_W - 8, 196, 2);

        tft.fillRoundRect((SCREEN_W - 120) / 2, 230, 120, 24, 5, 0x07E0);
        tft.setTextColor(TFT_BLACK, 0x07E0); tft.setTextDatum(MC_DATUM);
        tft.drawString("DONE", SCREEN_CX, 242, 2);
    };

    draw_settings();
    wait_release();

    while (true) {
        if (touch_is_pressed()) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            bool changed = false;

            if (ty >= 62 && ty < 90) {
                if (tx < SCREEN_CX) { pal = (pal + NUM_PALETTES - 1) % NUM_PALETTES; changed = true; }
                else if (tx > SCREEN_CX) { pal = (pal + 1) % NUM_PALETTES; changed = true; }
            }
            if (ty >= 122 && ty < 150) {
                if (tx < SCREEN_CX && fs > 0) { fs--; changed = true; }
                else if (tx > SCREEN_CX && fs < 4) { fs++; changed = true; }
            }
            if (ty >= 182 && ty < 210) {
                if (tx < SCREEN_CX && bl > 30) { bl -= 25; changed = true; }
                else if (tx > SCREEN_CX && bl < 255) { bl = min(255, bl + 25); changed = true; }
            }
            if (ty >= 230 && ty < 254 && tx >= (SCREEN_W - 120) / 2 && tx <= (SCREEN_W + 120) / 2) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                touch_save_settings(pal, fs, bl);
                wait_release();
                return;
            }

            if (changed) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                draw_settings();
                delay(200);
            }
        }
        delay(20);
    }
}
