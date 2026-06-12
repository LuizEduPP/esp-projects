#include "ui_icons.h"
#include "ui_icon_data.h"
#include "display.h"
#include "ui_theme.h"
#include <Arduino.h>

static const uint8_t* const ICON_ALPHA[UI_ICON_COUNT] = {
    ICO_GB_A, ICO_SD_A, ICO_CART_A, ICO_FOLDER_A, ICO_GEAR_A, ICO_GRID_A,
    ICO_CHEV_L_A, ICO_CHEV_R_A, ICO_PAUSE_A, ICO_PLAY_A, ICO_SAVE_A, ICO_LOAD_A,
    ICO_SLIDERS_A, ICO_TARGET_A, ICO_EXIT_A, ICO_PALETTE_A, ICO_SPEED_A, ICO_SUN_A,
    ICO_GLOBE_A, ICO_CHECK_A, ICO_X_A, ICO_SELECT_A, ICO_CROSS_A,
};

static uint8_t alpha_at(const uint8_t* alpha, int px, int py) {
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px >= UI_ICON_SIZE) px = UI_ICON_SIZE - 1;
    if (py >= UI_ICON_SIZE) py = UI_ICON_SIZE - 1;
    return pgm_read_byte(&alpha[py * UI_ICON_SIZE + px]);
}

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t a) {
    if (a >= 250) return fg;
    if (a <= 5) return bg;
    uint8_t fr = (fg >> 11) & 0x1F, fg_g = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    uint8_t br = (bg >> 11) & 0x1F, bg_g = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    uint8_t r = (fr * a + br * (255 - a)) / 255;
    uint8_t g = (fg_g * a + bg_g * (255 - a)) / 255;
    uint8_t b = (fb * a + bb * (255 - a)) / 255;
    return tft.color565(r << 3, g << 2, b << 3);
}

static uint8_t sample_alpha(const uint8_t* alpha, float u, float v) {
    int x0 = (int)u;
    int y0 = (int)v;
    if (x0 < 0) {
        x0 = 0;
        u = 0;
    }
    if (y0 < 0) {
        y0 = 0;
        v = 0;
    }
    if (x0 >= UI_ICON_SIZE - 1) {
        x0 = UI_ICON_SIZE - 2;
        u = (float)x0;
    }
    if (y0 >= UI_ICON_SIZE - 1) {
        y0 = UI_ICON_SIZE - 2;
        v = (float)y0;
    }

    float fx = u - x0;
    float fy = v - y0;
    float a00 = alpha_at(alpha, x0, y0);
    float a10 = alpha_at(alpha, x0 + 1, y0);
    float a01 = alpha_at(alpha, x0, y0 + 1);
    float a11 = alpha_at(alpha, x0 + 1, y0 + 1);
    float a0 = a00 + (a10 - a00) * fx;
    float a1 = a01 + (a11 - a01) * fx;
    return (uint8_t)(a0 + (a1 - a0) * fy);
}

static void blit_icon(int x, int y, int size, const uint8_t* alpha, uint16_t fg, uint16_t bg) {
    if (size <= 0) return;

    for (int dy = 0; dy < size; dy++) {
        float v = ((dy + 0.5f) * UI_ICON_SIZE / size) - 0.5f;
        for (int dx = 0; dx < size; dx++) {
            float u = ((dx + 0.5f) * UI_ICON_SIZE / size) - 0.5f;
            uint8_t a = sample_alpha(alpha, u, v);
            if (a < 32) continue;
            uint16_t c = (a >= 200) ? fg : blend565(fg, bg, a);
            tft.drawPixel(x + dx, y + dy, c);
        }
    }
}

void ui_icon_draw(int x, int y, int size, UiIcon icon, uint16_t color) {
    if (icon >= UI_ICON_COUNT || size < 8) return;
    blit_icon(x, y, size, ICON_ALPHA[icon], color, ui_theme_get()->surface);
}

void ui_icon_draw_t(int x, int y, int size, UiIcon icon) {
    ui_icon_draw(x, y, size, icon, ui_theme_get()->icon);
}

void ui_icon_draw_ok(int x, int y, int size, UiIcon icon) {
    ui_icon_draw(x, y, size, icon, ui_theme_get()->ok);
}

void ui_icon_draw_danger(int x, int y, int size, UiIcon icon) {
    ui_icon_draw(x, y, size, icon, ui_theme_get()->danger);
}
