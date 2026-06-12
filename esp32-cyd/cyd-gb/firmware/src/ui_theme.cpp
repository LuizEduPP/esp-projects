#include "ui_theme.h"
#include "emulator_bridge.h"
#include "display.h"

static UiTheme theme;

static uint16_t blend565(uint16_t fg, uint16_t bg, uint8_t pct) {
    if (pct >= 255) return fg;
    if (pct == 0) return bg;
    uint8_t fr = (fg >> 11) & 0x1F, fg_g = (fg >> 5) & 0x3F, fb = fg & 0x1F;
    uint8_t br = (bg >> 11) & 0x1F, bg_g = (bg >> 5) & 0x3F, bb = bg & 0x1F;
    uint8_t r = (fr * pct + br * (255 - pct)) / 255;
    uint8_t g = (fg_g * pct + bg_g * (255 - pct)) / 255;
    uint8_t b = (fb * pct + bb * (255 - pct)) / 255;
    return tft.color565(r << 3, g << 2, b << 3);
}

void ui_theme_apply(uint8_t palette_idx) {
    if (palette_idx >= NUM_PALETTES) palette_idx = 0;

    uint16_t p0 = emu_palette_color(palette_idx, 0);
    uint16_t p1 = emu_palette_color(palette_idx, 1);
    uint16_t p2 = emu_palette_color(palette_idx, 2);
    uint16_t p3 = emu_palette_color(palette_idx, 3);

    theme.pal[0] = p0;
    theme.pal[1] = p1;
    theme.pal[2] = p2;
    theme.pal[3] = p3;

    /* Base claro (wireframe) + acentos GB fortes */
    theme.bg         = 0xFFFF;
    theme.surface    = blend565(p0, 0xFFFF, 70);
    theme.border     = blend565(p2, 0xCE79, 220);
    theme.text_hi    = 0x2104;
    theme.text_lo    = 0xFFFF;
    theme.text_mute  = blend565(p2, 0x4208, 200);
    theme.icon       = p2;
    theme.row_hi     = blend565(p0, 0xC638, 130);
    theme.ok         = p0;
    theme.danger     = 0xF986;
    theme.accent     = p0;
    theme.card       = blend565(p1, 0xFFFF, 90);
    theme.card_sel   = blend565(p0, 0xFFFF, 75);
    theme.btn_a      = blend565(p0, 0xFFFF, 80);
    theme.btn_b      = blend565(p1, 0xFFFF, 80);
    theme.btn_a_bd   = p0;
    theme.btn_b_bd   = p1;
    theme.pause      = theme.surface;
    theme.pill       = blend565(p2, 0xE73C, 220);
    theme.stick_base = blend565(p2, 0xE73C, 180);
    theme.stick_ring = theme.border;
    theme.stick_knob = p1;
    theme.stick_act  = p0;
    theme.save_btn   = blend565(p3, 0x4208, 230);
}

const UiTheme* ui_theme_get() {
    return &theme;
}
