#include "ui_theme.h"
#include "display.h"

static ArcadeTheme s_theme;

/* Paleta Boreal — ardósia azul + joias (teal, lima, coral, âmbar) */
#define BOR_BG        0x08A4   /* #0D1520 fundo        */
#define BOR_SURFACE   0x10E5   /* #161F2E painéis      */
#define BOR_CARD      0x2188   /* #252F42 cartões      */
#define BOR_BORDER    0x394F   /* #3D5A80 aço           */
#define BOR_TEAL      0x4E78   /* #4ECDC4 destaque      */
#define BOR_CORAL     0xFB6D   /* #FF6B6B destaque 2    */
#define BOR_TEXT      0xEFF7   /* #E8EDF4 texto         */
#define BOR_MUTE      0x8CD6   /* #8B9CB3 texto sec.    */
#define BOR_LIME      0x9C6D   /* #95E06C ok / vida     */
#define BOR_AMBER     0xFEA0   /* #FFB347 recorde       */
#define BOR_RED       0xF986   /* #FF4757 perigo        */
#define BOR_SKY       0x5B99   /* #5B6FC8 faixa lista   */

static const uint16_t BOR_JEWELS[UI_THEME_BRICK_COUNT] = {
    BOR_TEAL, BOR_LIME, BOR_AMBER, BOR_CORAL, BOR_SKY,
};

uint16_t ui_rgb565(uint32_t rgb) {
    return tft.color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

const ArcadeTheme* ui_theme_get() {
    return &s_theme;
}

uint16_t ui_theme_brick_color(int index) {
    if (index < 0) index = 0;
    return BOR_JEWELS[index % UI_THEME_BRICK_COUNT];
}

void ui_theme_init() {
    s_theme.bg        = BOR_BG;
    s_theme.surface   = BOR_SURFACE;
    s_theme.card      = BOR_CARD;
    s_theme.card_sel  = BOR_SURFACE;
    s_theme.border    = BOR_BORDER;
    s_theme.accent    = BOR_TEAL;
    s_theme.accent_hi = BOR_CORAL;
    s_theme.text_hi   = BOR_TEXT;
    s_theme.text_mute = BOR_MUTE;
    s_theme.icon      = BOR_TEAL;
    s_theme.pause     = BOR_SURFACE;
    s_theme.play_bg   = BOR_BG;
    s_theme.ok        = BOR_LIME;
    s_theme.danger    = BOR_RED;
    s_theme.pal[0]    = BOR_LIME;
    s_theme.pal[1]    = BOR_AMBER;
    s_theme.pal[2]    = BOR_CORAL;
}
