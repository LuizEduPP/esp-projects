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

static uint8_t luma565(uint16_t c) {
    uint8_t r = (c >> 11) & 0x1F;
    uint8_t g = (c >> 5) & 0x3F;
    uint8_t b = c & 0x1F;
    return (uint8_t)((r * 76 + g * 150 + b * 29) >> 8);
}

static uint16_t lift(uint16_t c, uint8_t min_luma) {
    uint8_t l = luma565(c);
    if (l >= min_luma) return c;
    uint8_t pct = (uint8_t)min(255, (min_luma - l) * 18 + 120);
    return blend565(c, 0xFFFF, pct);
}

static uint16_t ink(uint16_t c) {
    if (luma565(c) <= 10) return c;
    return blend565(c, 0x2104, 210);
}

static void sort_shades(uint16_t p0, uint16_t p1, uint16_t p2, uint16_t p3,
                        uint16_t* darkest, uint16_t* mid, uint16_t* bright, uint16_t* lightest) {
    uint16_t s[4] = {p0, p1, p2, p3};
    for (int i = 0; i < 3; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (luma565(s[j]) < luma565(s[i])) {
                uint16_t t = s[i];
                s[i] = s[j];
                s[j] = t;
            }
        }
    }
    *darkest = s[0];
    *mid = s[1];
    *bright = s[2];
    *lightest = s[3];
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

    uint16_t dark, mid, bright, light;
    sort_shades(p0, p1, p2, p3, &dark, &mid, &bright, &light);

    /* Paleta escura (ex. Inverted): UI continua legivel, mas colorida */
    bool dim_palette = luma565(light) < 14;
    uint16_t lite = dim_palette ? blend565(light, 0xFFFF, 200) : lift(light, 14);
    uint16_t med  = dim_palette ? blend565(bright, 0xFFFF, 170) : lift(bright, 10);
    uint16_t pop  = dim_palette ? bright : mid;
    uint16_t deep = dim_palette ? blend565(dark, 0x2104, 80) : dark;

    theme.bg         = blend565(lite, 0xFFFF, 70);
    theme.surface    = blend565(lite, med, 200);
    theme.border     = blend565(deep, pop, 220);
    theme.text_hi    = ink(deep);
    theme.text_lo    = 0xFFFF;
    theme.text_mute  = blend565(deep, pop, 170);
    theme.icon       = ink(deep);
    theme.row_hi     = blend565(pop, med, 210);
    theme.ok         = pop;
    theme.danger     = 0xF986;
    theme.accent     = pop;
    theme.card       = blend565(med, lite, 190);
    theme.card_sel   = blend565(pop, med, 230);
    theme.btn_a      = blend565(pop, med, 240);
    theme.btn_b      = theme.btn_a;
    theme.btn_a_bd   = deep;
    theme.btn_b_bd   = deep;
    theme.pause      = blend565(med, lite, 160);
    theme.pill       = blend565(med, lite, 200);
    theme.pill_bd    = deep;
    theme.pill_icon  = (luma565(theme.pill) > 14) ? ink(deep) : 0xFFFF;
    theme.stick_base = blend565(med, lite, 180);
    theme.stick_ring = theme.border;
    theme.stick_knob = blend565(bright, lite, 220);
    theme.stick_act  = pop;
    theme.save_btn   = blend565(deep, pop, 240);
}

const UiTheme* ui_theme_get() {
    return &theme;
}
