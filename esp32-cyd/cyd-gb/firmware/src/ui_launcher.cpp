#include "ui_launcher.h"
#include "display.h"
#include "touch_input.h"
#include "emulator_bridge.h"
#include "hw_config.h"
#include "i18n.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_screens.h"
#include "ui_theme.h"
#include "sd_manager.h"
#include <Arduino.h>
#include <string.h>

#define TH ui_theme_get()
#define GRID_BOT UI_FTR_Y

static void draw_header(int cnt) {
    tft.fillRect(0, 0, SCREEN_W, UI_HDR_H, TH->surface);
    tft.drawFastHLine(0, UI_HDR_H - 1, SCREEN_W, TH->border);
    tft.fillRect(0, UI_HDR_H - 3, SCREEN_W, 2, TH->accent);

    ui_icon_draw_t(UI_HDR_ICON_X, 8, 24, UI_ICON_GRID);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(tr(STR_GAMES), UI_HDR_TEXT_X, 10, 2);

    char sub[20];
    snprintf(sub, sizeof(sub), tr(STR_ROMS_FMT), cnt);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString(sub, UI_HDR_TEXT_X, 26, 1);

    ui_icon_draw_t(UI_HDR_GEAR_X - 12, 8, 24, UI_ICON_GEAR);

    for (int c = 0; c < 4; c++)
        tft.fillRoundRect(124 + c * 14, 31, 12, 6, 2, TH->pal[c]);
}

static uint16_t cover_tint(const char* name) {
    uint32_t h = 0;
    for (const char* p = name; *p; p++) h = h * 31 + (uint8_t)*p;
    return emu_palette_color(emu_get_palette(), h % 4);
}

static void grid_cell_xy(int slot, int* x, int* y) {
    int col = slot % UI_GRID_COLS;
    int row = slot / UI_GRID_COLS;
    *x = UI_GRID_PAD + col * (UI_GRID_COL_W + UI_GRID_GAP);
    *y = UI_GRID_TOP + UI_GRID_PAD + row * (UI_GRID_ROW_H + UI_GRID_GAP);
}

static void grid_label_fit(const char* filename, char* out, size_t out_sz) {
    strncpy(out, filename, out_sz - 1);
    out[out_sz - 1] = '\0';
    char* dot = strrchr(out, '.');
    if (dot) *dot = '\0';

    const uint8_t font = 1;
    if (tft.textWidth(out, font) <= UI_GRID_LABEL_MAX_W) return;

    size_t n = strlen(out);
    int ell_w = tft.textWidth("...", font);
    while (n > 0 && tft.textWidth(out, font) + ell_w > UI_GRID_LABEL_MAX_W)
        out[--n] = '\0';
    if (n > 0 && n + 3 < out_sz) {
        out[n] = '.';
        out[n + 1] = '.';
        out[n + 2] = '.';
        out[n + 3] = '\0';
    }
}

static void draw_grid_cell(RomEntry* r, int slot, bool sel) {
    int cx, cy;
    grid_cell_xy(slot, &cx, &cy);

    uint16_t bg = sel ? TH->card_sel : TH->card;
    tft.fillRoundRect(cx, cy, UI_GRID_COL_W, UI_GRID_ROW_H, 4, bg);
    tft.drawRoundRect(cx, cy, UI_GRID_COL_W, UI_GRID_ROW_H, 4, sel ? TH->accent : TH->border);

    int cover_x = cx + 4;
    int cover_y = cy + 4;
    tft.fillRoundRect(cover_x, cover_y, UI_GRID_COVER_W, UI_GRID_COVER_H, 3, TH->border);
    if (!sd_draw_rom_cover(r, cover_x, cover_y, UI_GRID_COVER_W, UI_GRID_COVER_H)) {
        tft.fillRoundRect(cover_x, cover_y, UI_GRID_COVER_W, UI_GRID_COVER_H, 3, cover_tint(r->filename));
        ui_icon_draw_t(cover_x + 32, cover_y + 20, 36, UI_ICON_CART);
    }

    char fname[24];
    grid_label_fit(r->filename, fname, sizeof(fname));
    int label_y = cy + UI_GRID_LABEL_Y;
    tft.fillRect(cx + 4, label_y - 6, UI_GRID_COL_W - 8, 12, bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_mute, bg);
    tft.drawString(fname, cx + UI_GRID_COL_W / 2, label_y, 1);
}

static void draw_footer(int pg, int tp) {
    tft.fillRect(0, UI_FTR_Y, SCREEN_W, UI_FTR_H, TH->surface);
    tft.drawFastHLine(0, UI_FTR_Y, SCREEN_W, TH->border);
    tft.drawFastVLine(SCREEN_CX, UI_FTR_Y, UI_FTR_H, TH->border);

    ui_icon_draw(36, UI_FTR_Y + 12, 24, UI_ICON_CHEV_L, TH->text_hi);

    if (tp > 1) {
        char ps[12];
        snprintf(ps, sizeof(ps), "%d/%d", pg + 1, tp);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->text_mute, TH->surface);
        tft.drawString(ps, SCREEN_CX, UI_FTR_Y + 24, 1);
    }

    ui_icon_draw(168, UI_FTR_Y + 12, 24, UI_ICON_CHEV_R, TH->text_hi);
}

static void draw_grid(RomEntry* roms, int cnt, int pg, int sel) {
    tft.fillRect(0, UI_GRID_TOP, SCREEN_W, GRID_BOT - UI_GRID_TOP, TH->bg);
    int base = pg * UI_GRID_ITEMS;
    int end = min(base + UI_GRID_ITEMS, cnt);
    for (int i = base; i < end; i++)
        draw_grid_cell(&roms[i], i - base, i == sel);
    draw_footer(pg, (cnt + UI_GRID_ITEMS - 1) / UI_GRID_ITEMS);
}

static bool hit_gear(int16_t tx, int16_t ty) {
    return tx >= UI_HDR_GEAR_ZONE_X && tx < UI_HDR_GEAR_ZONE_X + UI_HDR_GEAR_ZONE_W
        && ty >= 0 && ty < UI_HDR_H;
}

static int grid_hit(int16_t tx, int16_t ty, int pg, int cnt) {
    if (ty < UI_GRID_TOP + UI_GRID_PAD || ty >= GRID_BOT) return -1;
    for (int slot = 0; slot < UI_GRID_ITEMS; slot++) {
        int cx, cy;
        grid_cell_xy(slot, &cx, &cy);
        if (tx >= cx && tx < cx + UI_GRID_COL_W && ty >= cy && ty < cy + UI_GRID_ROW_H) {
            int idx = pg * UI_GRID_ITEMS + slot;
            if (idx < cnt) return idx;
        }
    }
    return -1;
}

int launcher_show(RomEntry* roms, int cnt) {
    int pg = 0;
    ui_sync();
    tft.fillScreen(TH->bg);
    draw_header(cnt);

    if (cnt == 0) {
        ui_draw_no_roms_empty();
        while (true) delay(1000);
    }

    draw_grid(roms, cnt, pg, -1);
    ui_wait_release();

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
            if (hit_gear(tx, ty)) {
                cfg_pending = true;
            } else {
                int idx = grid_hit(tx, ty, pg, cnt);
                if (idx >= 0 && idx != pending_sel) {
                    pending_sel = idx;
                    draw_grid(roms, cnt, pg, pending_sel);
                }
            }
        } else if (was_pressed) {
            if (cfg_pending) return -3;
            if (pending_sel >= 0) return pending_sel;
            if (last_ty >= UI_FTR_Y) {
                int tp = (cnt + UI_GRID_ITEMS - 1) / UI_GRID_ITEMS;
                if (last_tx < SCREEN_CX && pg > 0) {
                    pg--;
                    draw_grid(roms, cnt, pg, -1);
                } else if (last_tx >= SCREEN_CX && pg < tp - 1) {
                    pg++;
                    draw_grid(roms, cnt, pg, -1);
                }
            }
            cfg_pending = false;
            pending_sel = -1;
        }
        was_pressed = pressed;
        delay(10);
    }
}

static const UiIcon pause_icons[6] = {
    UI_ICON_PLAY, UI_ICON_SAVE, UI_ICON_LOAD,
    UI_ICON_SLIDERS, UI_ICON_TARGET, UI_ICON_EXIT
};

int launcher_ingame_menu() {
    ui_sync();
    ui_bar_header(UI_PAUSE_HDR, UI_ICON_PAUSE, tr(STR_PAUSED), 130);

    const int MI = 6;
    const StringId lb_ids[MI] = {
        STR_MENU_RESUME, STR_MENU_SAVE, STR_MENU_LOAD,
        STR_MENU_SETTINGS, STR_MENU_CALIBRATE, STR_MENU_QUIT
    };

    for (int i = 0; i < MI; i++)
        ui_menu_row(UI_PAUSE_HDR + i * UI_PAUSE_ROW_H, UI_PAUSE_ROW_H,
                    pause_icons[i], tr(lb_ids[i]), i == 0, i == 5);

    ui_wait_release();

    int hl = 0;
    bool was_pressed = false;
    while (true) {
        bool pressed = touch_is_pressed();
        if (pressed) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            (void)tx;
            if (ty >= UI_PAUSE_HDR) {
                int row = (ty - UI_PAUSE_HDR) / UI_PAUSE_ROW_H;
                if (row >= 0 && row < MI && row != hl) {
                    ui_menu_row(UI_PAUSE_HDR + hl * UI_PAUSE_ROW_H, UI_PAUSE_ROW_H,
                                pause_icons[hl], tr(lb_ids[hl]), false, hl == 5);
                    ui_menu_row(UI_PAUSE_HDR + row * UI_PAUSE_ROW_H, UI_PAUSE_ROW_H,
                                pause_icons[row], tr(lb_ids[row]), true, row == 5);
                    hl = row;
                }
            }
        } else if (was_pressed) {
            int s = hl;
            switch (s) {
                case 0: return 0;
                case 1: return 1;
                case 2: return 2;
                case 3: return 5;
                case 4: return 4;
                case 5: return 3;
            }
        }
        was_pressed = pressed;
        delay(15);
    }
}

static const UiIcon set_icons[4] = {
    UI_ICON_PALETTE, UI_ICON_SPEED, UI_ICON_SUN, UI_ICON_GLOBE
};

static void draw_set_row(int y, int h, int row, StringId label_id, const char* value, bool swatches, uint8_t pal) {
    tft.fillRect(0, y, SCREEN_W, h, TH->surface);
    ui_icon_draw_t(UI_SET_ICON_X, y + 8, 20, set_icons[row]);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString(tr(label_id), UI_SET_LABEL_X, y + (swatches ? 8 : 10), 1);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(value, SCREEN_CX, y + (swatches ? 26 : 30), 2);
    if (row < 2) {
        ui_icon_draw(24, y + h - 28, 24, UI_ICON_CHEV_L, TH->text_hi);
        ui_icon_draw(192, y + h - 28, 24, UI_ICON_CHEV_R, TH->text_hi);
    }
    if (swatches) {
        for (int c = 0; c < 4; c++)
            tft.fillRoundRect(48 + c * 36, y + 44, 28, 8, 2, TH->pal[c]);
    }
}

void launcher_settings_menu() {
    uint8_t pal = emu_get_palette();
    uint8_t fs = emu_get_frame_skip();
    uint8_t bl = display_get_backlight();
    uint8_t lang = i18n_get_lang();
    emu_set_palette(pal);

    auto draw_settings = [&]() {
        tft.fillScreen(TH->bg);
        ui_bar_header(UI_SET_HDR, UI_ICON_GEAR, tr(STR_SETTINGS), 130);

        char palstr[24];
        snprintf(palstr, sizeof(palstr), "%s", emu_get_palette_name(pal));
        draw_set_row(UI_SET_PAL_Y, UI_SET_PAL_H, 0, STR_PALETTE, palstr, true, pal);

        char fss[8];
        snprintf(fss, sizeof(fss), "%d", fs);
        draw_set_row(UI_SET_ROW2_Y, UI_SET_ROW_H, 1, STR_FRAME_SKIP, fss, false, pal);

        char bls[8];
        snprintf(bls, sizeof(bls), "%d%%", bl * 100 / 255);
        draw_set_row(UI_SET_ROW3_Y, UI_SET_ROW_H, 2, STR_BRIGHTNESS, bls, false, pal);

        draw_set_row(UI_SET_ROW4_Y, UI_SET_ROW_H, 3, STR_LANGUAGE, i18n_lang_label(lang), false, pal);

        tft.fillRect(0, UI_SET_SAVE_Y, SCREEN_W, UI_SET_SAVE_H, TH->save_btn);
        ui_icon_draw_ok(52, UI_SET_SAVE_Y + 24, 20, UI_ICON_CHECK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_WHITE, TH->save_btn);
        tft.drawString(tr(STR_SAVE_BACK), SCREEN_CX, UI_SET_SAVE_Y + UI_SET_SAVE_H / 2, 2);
    };

    auto row_y = [&](int i) -> int {
        static const int ys[] = {UI_SET_PAL_Y, UI_SET_ROW2_Y, UI_SET_ROW3_Y, UI_SET_ROW4_Y};
        return (i >= 0 && i <= 3) ? ys[i] : -1;
    };

    auto row_h = [&](int i) -> int {
        return i == 0 ? UI_SET_PAL_H : UI_SET_ROW_H;
    };

    auto hit_row = [&](int i, int16_t tx, int16_t ty) {
        int y = row_y(i);
        return y >= 0 && ty >= y && ty < y + row_h(i);
    };

    draw_settings();
    ui_wait_release();

    while (true) {
        if (touch_is_pressed()) {
            int16_t tx = touch_get_x(), ty = touch_get_y();
            bool changed = false;

            for (int i = 0; i < 4; i++) {
                if (!hit_row(i, tx, ty)) continue;
                if (tx < UI_SET_TAP_W) {
                    if (i == 0) pal = (pal + NUM_PALETTES - 1) % NUM_PALETTES;
                    else if (i == 1 && fs > 0) fs--;
                    else if (i == 2 && bl > 30) bl -= 25;
                    else if (i == 3) lang = (lang + LANG_COUNT - 1) % LANG_COUNT;
                    changed = true;
                } else if (tx >= SCREEN_W - UI_SET_TAP_W) {
                    if (i == 0) pal = (pal + 1) % NUM_PALETTES;
                    else if (i == 1 && fs < 4) fs++;
                    else if (i == 2 && bl < 255) bl = min(255, bl + 25);
                    else if (i == 3) lang = (lang + 1) % LANG_COUNT;
                    changed = true;
                }
                if (changed && i == 3) i18n_set_lang(lang);
                break;
            }

            if (ty >= UI_SET_SAVE_Y && ty < UI_SET_SAVE_Y + UI_SET_SAVE_H) {
                emu_set_palette(pal);
                emu_set_frame_skip(fs);
                display_set_backlight(bl);
                i18n_set_lang(lang);
                touch_save_settings(pal, fs, bl, lang);
                ui_show_toast(tr(STR_SAVED), TH->ok);
                ui_wait_release();
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
