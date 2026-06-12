#include "ui_screens.h"
#include "display.h"
#include "hw_config.h"
#include "i18n.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_theme.h"
#include <Arduino.h>

#define TH ui_theme_get()

void ui_draw_splash(int progress_pct) {
    ui_sync();
    tft.fillScreen(TH->bg);
    tft.fillRoundRect(UI_SPLASH_PANEL_X, UI_SPLASH_PANEL_Y,
                      UI_SPLASH_PANEL_W, UI_SPLASH_PANEL_H, 12, TH->card);
    tft.drawRoundRect(UI_SPLASH_PANEL_X, UI_SPLASH_PANEL_Y,
                      UI_SPLASH_PANEL_W, UI_SPLASH_PANEL_H, 12, TH->accent);
    ui_icon_draw_t(96, 92, 48, UI_ICON_GB);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString("CYD-GB", SCREEN_CX, UI_STATUS_TITLE_Y, 2);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(tr(STR_SPLASH_SUB), SCREEN_CX, UI_STATUS_SUB_Y, 2);
    ui_progress_bar(UI_STATUS_BAR_X, UI_STATUS_BAR_Y,
                    UI_STATUS_BAR_W, UI_STATUS_BAR_H, progress_pct);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(tr(STR_BOOT), SCREEN_CX, UI_STATUS_BAR_HINT, 1);
}

void ui_animate_splash(uint32_t duration_ms) {
    uint32_t t0 = millis();
    while (millis() - t0 < duration_ms) {
        int pct = (int)((millis() - t0) * 100 / duration_ms);
        ui_draw_splash(pct);
        delay(40);
    }
    ui_draw_splash(100);
}

void ui_draw_sd_error() {
    ui_sync();
    ui_status_result(UI_ICON_SD, STR_SD_ERROR, STR_SD_HINT, STR_RESET_DEVICE, false);
}

void ui_draw_loading(const char* title, int progress_pct) {
    tft.fillScreen(TH->bg);
    tft.fillRoundRect(88, 92, 64, 48, 6, TH->surface);
    tft.drawRoundRect(88, 92, 64, 48, 6, TH->border);
    ui_icon_draw_t(100, 100, 40, UI_ICON_CART);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString(tr(STR_LOADING), SCREEN_CX, UI_STATUS_TITLE_Y, 2);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(title, SCREEN_CX, UI_STATUS_SUB_Y, 2);
    ui_progress_bar(UI_STATUS_BAR_X, UI_STATUS_BAR_Y,
                    UI_STATUS_BAR_W, UI_STATUS_BAR_H, progress_pct);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString(tr(STR_READING_ROM), SCREEN_CX, UI_STATUS_BAR_HINT, 1);
}

bool ui_loading_run(const char* title, bool (*load_fn)(void)) {
    uint32_t t0 = millis();
    while (millis() - t0 < 400) {
        int pct = (int)((millis() - t0) * 65 / 400);
        ui_draw_loading(title, pct);
        delay(30);
    }
    bool ok = load_fn ? load_fn() : true;
    ui_draw_loading(title, 100);
    delay(80);
    return ok;
}

void ui_draw_no_roms_empty() {
    ui_sync();
    ui_status_result(UI_ICON_FOLDER, STR_NO_ROMS, STR_NO_ROMS_HINT, STR_NO_ROMS_ADD, false);
}

void ui_show_toast(const char* msg, uint16_t color) {
    ui_toast(msg, color);
}
