#include "ui_theme.h"
#include "emulator_bridge.h"

static UiTheme theme;

void ui_theme_apply(uint8_t palette_idx) {
    if (palette_idx >= NUM_PALETTES) palette_idx = 0;

    theme.bg = emu_palette_color(palette_idx, 3);
    theme.surface = emu_palette_color(palette_idx, 2);
    theme.border = emu_palette_color_dim(palette_idx, 2, 70, 100);
    theme.accent = emu_palette_color(palette_idx, 0);
    theme.mute = emu_palette_color(palette_idx, 1);
    theme.card = emu_palette_color(palette_idx, 2);
    theme.card_sel = emu_palette_color(palette_idx, 1);
    theme.btn_a = emu_palette_color(palette_idx, 0);
    theme.btn_b = emu_palette_color(palette_idx, 1);
    theme.pause = emu_palette_color(palette_idx, 0);
    theme.pill = emu_palette_color(palette_idx, 2);
    theme.stick_base = emu_palette_color(palette_idx, 2);
    theme.stick_ring = emu_palette_color(palette_idx, 1);
    theme.stick_knob = emu_palette_color(palette_idx, 1);
    theme.stick_act = emu_palette_color(palette_idx, 0);
    theme.text_hi = emu_palette_color(palette_idx, 0);
    theme.text_lo = emu_palette_color(palette_idx, 3);
    theme.text_mute = emu_palette_color(palette_idx, 1);
    theme.menu_primary = emu_palette_color(palette_idx, 0);
    theme.menu_danger = emu_palette_color_dim(palette_idx, 0, 85, 100);
}

const UiTheme* ui_theme_get() {
    return &theme;
}
