#include "ui_draw.h"
#include "display.h"
#include "game_catalog.h"
#include "hw_config.h"
#include "ui_icons.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>

static uint8_t clamp_u8(int v) {
    if (v < 0) return 0;
    if (v > 255) return 255;
    return (uint8_t)v;
}

uint16_t ui_tint565(uint16_t color, int8_t pct) {
    uint8_t r = ((color >> 11) & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x3F) << 2;
    uint8_t b = (color & 0x1F) << 3;
    r |= r >> 5;
    g |= g >> 6;
    b |= b >> 5;
    return ui_shade565((uint32_t)r << 16 | (uint32_t)g << 8 | b, pct);
}

uint16_t ui_shade565(uint32_t rgb, int8_t pct) {
    const uint8_t r = (rgb >> 16) & 0xFF;
    const uint8_t g = (rgb >> 8) & 0xFF;
    const uint8_t b = rgb & 0xFF;
    if (pct >= 0) {
        const uint8_t f = clamp_u8(pct);
        return tft.color565(r + ((255 - r) * f) / 255,
                            g + ((255 - g) * f) / 255,
                            b + ((255 - b) * f) / 255);
    }
    const uint8_t f = clamp_u8(-pct);
    return tft.color565((r * (255 - f)) / 255,
                        (g * (255 - f)) / 255,
                        (b * (255 - f)) / 255);
}

void ui_draw_grid_icon(int x, int y, int size, uint16_t color, uint16_t bg) {
    const int pad = size / 8 < 2 ? 2 : size / 8;
    const int cell = (size - pad * 3) / 2;
    tft.fillRoundRect(x, y, size, size, 3, bg);
    for (int row = 0; row < 2; row++) {
        for (int col = 0; col < 2; col++) {
            const int cx = x + pad + col * (cell + pad);
            const int cy = y + pad + row * (cell + pad);
            tft.fillRoundRect(cx, cy, cell, cell, 2, color);
        }
    }
}

void ui_fill_screen_bg() {
    tft.fillScreen(ui_theme_get()->bg);
}

void ui_draw_app_header(UiIcon icon, const char* title, const char* subtitle) {
    const ArcadeTheme* th = ui_theme_get();
    tft.fillRect(0, 0, SCREEN_W, UI_HDR_H, th->surface);
    tft.drawFastHLine(0, UI_HDR_H - 1, SCREEN_W, th->border);
    ui_icon_draw(UI_PAD, 12, 24, icon, th->icon);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(th->text_hi, th->surface);
    tft.drawString(title, UI_PAD + 32, 12, 2);
    if (subtitle && subtitle[0]) {
        tft.setTextColor(th->text_mute, th->surface);
        tft.drawString(subtitle, UI_PAD + 32, 30, 1);
    }
}

void ui_draw_app_header_btn(UiIcon icon, const char* title, const char* subtitle,
                            UiIcon btn_icon, int btn_x) {
    const ArcadeTheme* th = ui_theme_get();
    ui_draw_app_header(icon, title, subtitle);
    tft.fillRoundRect(btn_x, 10, 36, 28, UI_CARD_R, th->card);
    ui_icon_draw(btn_x + 6, 12, 24, btn_icon, th->icon);
}

void ui_draw_splash(const char* title, const char* subtitle) {
    (void)title;
    const ArcadeTheme* th = ui_theme_get();
    ui_fill_screen_bg();

    const int sz = 80;
    const int px = (SCREEN_W - sz) / 2;
    const int py = 72;
    tft.fillRoundRect(px, py, sz, sz, 16, th->card);
    tft.drawRoundRect(px, py, sz, sz, 16, th->border);
    ui_draw_grid_icon(px + 16, py + 16, 48, th->accent, th->card);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(th->text_hi, th->bg);
    tft.drawString("CYD ARCADE", SCREEN_CX, 188, 4);
    tft.setTextColor(th->text_mute, th->bg);
    char sub[24];
    if (subtitle && subtitle[0])
        tft.drawString(subtitle, SCREEN_CX, 218, 2);
    else {
        snprintf(sub, sizeof(sub), "%d jogos", game_catalog_count());
        tft.drawString(sub, SCREEN_CX, 218, 2);
    }

    tft.fillRoundRect(UI_PAD, SPLASH_BAR_Y, UI_CONTENT_W, 4, 2, th->card);
    tft.fillRoundRect(UI_PAD, SPLASH_BAR_Y, UI_CONTENT_W * 2 / 3, 4, 2, th->accent);
}
