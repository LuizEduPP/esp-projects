#include "ui_theme.h"
#include "display.h"

static ArcadeTheme s_theme;

static uint16_t pal565(uint32_t rgb) {
    return tft.color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

uint16_t ui_rgb565(uint32_t rgb) {
    return pal565(rgb);
}

uint16_t ui_theme_game_color(uint32_t rgb) {
    return pal565(rgb);
}

const ArcadeTheme* ui_theme_get() {
    return &s_theme;
}

uint16_t ui_theme_brick_color(int index) {
    static const uint32_t jewels[UI_THEME_BRICK_COUNT] = {
        0x5B8DF2, 0x7B68EE, 0x9B59B6, 0x6A5ACD, 0x4169E1,
    };
    if (index < 0) index = 0;
    return pal565(jewels[index % UI_THEME_BRICK_COUNT]);
}

/* UI roxo + azul — jogos usam paletas clássicas próprias */
void ui_theme_init() {
    s_theme.bg         = pal565(0x1E1040u);
    s_theme.surface    = pal565(0x2A1860u);
    s_theme.card       = pal565(0x362070u);
    s_theme.border     = pal565(0x6B5CAEu);
    s_theme.accent     = pal565(0x5B8DF2u);
    s_theme.accent_hi  = pal565(0xB57BFFu);
    s_theme.text_hi    = pal565(0xF0EEF8u);
    s_theme.text_mute  = pal565(0xA89CC8u);
    s_theme.icon       = pal565(0x7B9FFFu);
    s_theme.play_bg    = pal565(0x1E1040u);
    s_theme.play_field = 0x0000;
    s_theme.play_grid  = pal565(0x4A4080u);
    s_theme.life_on    = pal565(0x5B8DF2u);
    s_theme.life_off   = pal565(0x4A4080u);
    s_theme.ok         = pal565(0x5B8DF2u);
    s_theme.danger     = pal565(0xE879F9u);
    s_theme.pal[0]     = pal565(0x5B8DF2u);
    s_theme.pal[1]     = pal565(0xB57BFFu);
    s_theme.pal[2]     = pal565(0x7B9FFFu);
}
