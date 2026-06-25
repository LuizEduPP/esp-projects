#include "ui_draw.h"
#include "display.h"
#include "emulator_bridge.h"
#include "hw_config.h"
#include "i18n.h"
#include "touch_input.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>

#define TH ui_theme_get()

void ui_sync(void) {
    ui_theme_apply(emu_get_palette());
}

void ui_clear(void) {
    tft.fillScreen(TH->bg);
}

void ui_wait_release(void) {
    while (touch_is_pressed()) delay(10);
    delay(80);
}

void ui_bar_header(int h, UiIcon icon, const char* title, int title_x) {
    tft.fillRect(0, 0, SCREEN_W, h, TH->surface);
    tft.drawFastHLine(0, h - 1, SCREEN_W, TH->border);
    tft.fillRect(0, h - 4, SCREEN_W, 3, TH->accent);
    ui_icon_draw_t(98, 4, 24, icon);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(title, title_x, 8, 2);
}

void ui_menu_row(int y, int h, UiIcon icon, const char* label, bool hl, bool danger) {
    uint16_t bg = hl ? TH->row_hi : TH->surface;
    tft.fillRect(0, y, SCREEN_W, h, bg);
    if (danger)
        ui_icon_draw_danger(UI_PAUSE_ICON_X, y + 12, 24, icon);
    else
        ui_icon_draw_t(UI_PAUSE_ICON_X, y + 12, 24, icon);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(danger ? TH->danger : TH->text_hi, bg);
    tft.drawString(label, UI_PAUSE_TEXT_X, y + h / 2, 2);
}

void ui_progress_bar(int x, int y, int w, int h, int pct) {
    ui_progress_bar_update(x, y, w, h, pct);
}

void ui_progress_bar_update(int x, int y, int w, int h, int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    tft.fillRoundRect(x, y, w, h, h / 2, TH->pal[2]);
    int fw = (w * pct) / 100;
    if (fw > 0) tft.fillRoundRect(x, y, fw, h, h / 2, TH->accent);
}

void ui_status_body(UiIcon icon, int icon_sz, StringId title, StringId sub, StringId hint) {
    tft.fillScreen(TH->bg);
    ui_icon_draw_t(UI_STATUS_ICON_X, UI_STATUS_ICON_Y, icon_sz, icon);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString(tr(title), SCREEN_CX, UI_STATUS_TITLE_Y, 2);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(tr(sub), SCREEN_CX, UI_STATUS_SUB_Y, 2);
    if (hint < STR_COUNT)
        tft.drawString(tr(hint), SCREEN_CX, UI_STATUS_HINT_Y, 1);
}

void ui_status_result(UiIcon icon, StringId title, StringId sub, StringId hint, bool ok) {
    tft.fillScreen(TH->bg);
    if (ok)
        ui_icon_draw_ok(UI_STATUS_ICON_X, UI_STATUS_ICON_Y, UI_STATUS_ICON_SZ, icon);
    else
        ui_icon_draw_danger(UI_STATUS_ICON_X, UI_STATUS_ICON_Y, UI_STATUS_ICON_SZ, icon);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ok ? TH->ok : TH->danger, TH->bg);
    tft.drawString(tr(title), SCREEN_CX, UI_STATUS_TITLE_Y, 2);
    if (sub < STR_COUNT) {
        tft.setTextColor(TH->text_mute, TH->bg);
        tft.drawString(tr(sub), SCREEN_CX, UI_STATUS_SUB_Y, 2);
    }
    if (hint < STR_COUNT) {
        tft.setTextColor(TH->text_mute, TH->bg);
        tft.drawString(tr(hint), SCREEN_CX, UI_STATUS_HINT_Y, 1);
    }
}

void ui_toast(const char* msg, uint16_t color) {
    tft.fillRoundRect(UI_TOAST_X, UI_TOAST_Y, UI_TOAST_W, UI_TOAST_H, 6, TH->surface);
    tft.drawRoundRect(UI_TOAST_X, UI_TOAST_Y, UI_TOAST_W, UI_TOAST_H, 6, TH->border);
    ui_icon_draw_ok(UI_TOAST_X + 8, UI_TOAST_Y + 10, 24, UI_ICON_CHECK);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(color, TH->surface);
    tft.drawString(msg, UI_TOAST_X + 56, UI_TOAST_Y + 10, 2);
    delay(700);
}
