#include "ui_draw.h"
#include "display.h"
#include "engine.h"
#include "hw_config.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <math.h>

#define TH ui_theme_get()

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

void ui_draw_gear(int x, int y, int size, uint16_t color, uint16_t bg) {
    const int cx = x + size / 2;
    const int cy = y + size / 2;
    const int r = size / 2 - 1;
    tft.fillRoundRect(x, y, size, size, size / 4, bg);
    tft.drawRoundRect(x, y, size, size, size / 4, TH->border);

    for (int i = 0; i < 8; i++) {
        const float a = (float)i * 0.785398f;
        const int tx = cx + (int)(cosf(a) * r * 0.72f);
        const int ty = cy + (int)(sinf(a) * r * 0.72f);
        const int tooth = size / 7 < 2 ? 2 : size / 7;
        tft.fillCircle(tx, ty, tooth, color);
    }
    tft.drawCircle(cx, cy, r - 2, color);
    const int hub = size / 5 < 3 ? 3 : size / 5;
    tft.fillCircle(cx, cy, hub, bg);
    tft.drawCircle(cx, cy, hub, color);
}

void ui_draw_nav_btn(int cx, int cy, int r, bool right, bool enabled) {
    const uint16_t bg = enabled ? TH->accent : TH->card;
    const uint16_t fg = enabled ? TH->text_hi : TH->border;
    tft.fillCircle(cx, cy, r, bg);
    if (enabled)
        tft.drawCircle(cx, cy, r, TH->accent_hi);
    else
        tft.drawCircle(cx, cy, r, TH->border);
    const int ch = r + 4;
    ui_draw_chevron(right ? cx - ch / 2 - 2 : cx - ch / 2 + 2, cy - ch / 2, ch, right, fg);
}

void ui_draw_score_badge(int cx, int y, const char* text, uint16_t fg, uint16_t bg) {
    if (!text || !text[0]) return;
    int w = (int)tft.textWidth(text, 1) + 12;
    if (w < 40) w = 40;
    const int x = cx - w / 2;
    tft.fillRoundRect(x, y, w, 14, 4, bg);
    tft.drawRoundRect(x, y, w, 14, 4, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(text, cx, y + 7, 1);
}

void ui_draw_chevron(int x, int y, int size, bool right, uint16_t color) {
    const int cy = y + size / 2;
    if (right) {
        tft.drawLine(x, y, x + size, cy, color);
        tft.drawLine(x + size, cy, x, y + size, color);
        tft.drawLine(x + 1, y, x + size, cy, color);
        tft.drawLine(x + size, cy, x + 1, y + size, color);
    } else {
        tft.drawLine(x + size, y, x, cy, color);
        tft.drawLine(x, cy, x + size, y + size, color);
        tft.drawLine(x + size, y + 1, x + 1, cy, color);
        tft.drawLine(x + 1, cy, x + size, y + size - 1, color);
    }
}

void ui_draw_pause_btn(int x, int y, int w, int h, uint16_t fg, uint16_t bg, uint16_t border) {
    tft.fillRoundRect(x, y, w, h, 4, bg);
    tft.drawRoundRect(x, y, w, h, 4, border);
    const int bar_w = w / 7 < 3 ? 3 : w / 7;
    const int gap = w / 6 < 4 ? 4 : w / 6;
    const int cx = x + w / 2;
    const int top = y + h / 4;
    const int bot = y + h - h / 4;
    tft.fillRoundRect(cx - gap - bar_w, top, bar_w, bot - top, 1, fg);
    tft.fillRoundRect(cx + gap, top, bar_w, bot - top, 1, fg);
}

static void cover_snake(int x, int y, int w, int h, int brick_idx) {
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    const int cs = w / 14 < 6 ? 6 : w / 14;
    int px = x + w / 5;
    int py = y + h / 2;
    for (int i = 0; i < 5; i++)
        tft.fillRoundRect(px + i * (cs + 1), py - cs / 2, cs, cs, 2,
                          ui_theme_brick_color(brick_idx + 2));
    tft.fillCircle(x + w * 4 / 5, y + h / 3, cs / 2 + 1, ui_theme_brick_color(0));
}

static void cover_tetris(int x, int y, int w, int h, int brick_idx) {
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    const int cs = w / 12 < 5 ? 5 : w / 12;
    const int ox = x + (w - cs * 4) / 2;
    const int oy = y + (h - cs * 3) / 2;
    for (int i = 0; i < 4; i++)
        tft.fillRoundRect(ox + i * (cs + 1), oy + cs + 2, cs, cs, 1,
                          ui_theme_brick_color(brick_idx + i));
    for (int i = 0; i < 3; i++)
        tft.fillRoundRect(ox + cs + 1 + i * (cs + 1), oy, cs, cs, 1,
                          ui_theme_brick_color(brick_idx + i + 1));
}

static void cover_simon(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    const int hw = (w - 6) / 2;
    const int hh = (h - 6) / 2;
    tft.fillRoundRect(x + 2, y + 2, hw, hh, 3, ui_theme_brick_color(0));
    tft.fillRoundRect(x + 4 + hw, y + 2, hw, hh, 3, ui_theme_brick_color(3));
    tft.fillRoundRect(x + 2, y + 4 + hh, hw, hh, 3, ui_theme_brick_color(2));
    tft.fillRoundRect(x + 4 + hw, y + 4 + hh, hw, hh, 3, ui_theme_brick_color(4));
}

static void cover_mines(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    const int cs = (w - 8) / 4;
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            const int cx = x + 4 + c * cs;
            const int cy = y + 4 + r * cs;
            tft.fillRect(cx, cy, cs - 1, cs - 1, TH->border);
            if (r == 1 && c == 2) {
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(TH->accent, TH->border);
                tft.drawString("2", cx + cs / 2, cy + cs / 2, 1);
            }
        }
    }
}

static void cover_dodge(int x, int y, int w, int h, int brick_idx) {
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    tft.drawFastVLine(x + w / 3, y + 4, h - 8, TH->accent);
    tft.drawFastVLine(x + w * 2 / 3, y + 4, h - 8, TH->accent);
    tft.fillRoundRect(x + w / 2 - 8, y + h - 12, 16, 7, 2, ui_theme_brick_color(brick_idx));
    tft.fillRoundRect(x + w / 6 - 6, y + 10, 12, 8, 2, ui_theme_brick_color(0));
    tft.fillRoundRect(x + w * 5 / 6 - 6, y + 20, 12, 8, 2, ui_theme_brick_color(3));
}

static void cover_reaction(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    tft.fillCircle(x + w / 2, y + h / 2, w / 5, TH->ok);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->bg);
    tft.drawString("!", x + w / 2, y + h / 2, 2);
}

static void cover_rps(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    tft.fillCircle(x + w / 3, y + h / 2, 5, TH->border);
    tft.fillRect(x + w / 2 - 2, y + h / 3, 4, h / 3, TH->accent);
    tft.drawLine(x + w * 2 / 3 - 4, y + h / 3, x + w * 2 / 3 + 4, y + h * 2 / 3, TH->danger);
    tft.drawLine(x + w * 2 / 3 + 4, y + h / 3, x + w * 2 / 3 - 4, y + h * 2 / 3, TH->danger);
}

static void cover_velha(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    const int cs = (w - 10) / 3;
    const int ox = x + (w - cs * 3) / 2;
    const int oy = y + (h - cs * 3) / 2;
    for (int i = 1; i < 3; i++) {
        tft.fillRect(ox + i * cs - 1, oy, 2, cs * 3, TH->border);
        tft.fillRect(ox, oy + i * cs - 1, cs * 3, 2, TH->border);
    }
    tft.drawLine(ox + cs / 4, oy + cs / 4, ox + cs * 3 / 4, oy + cs * 3 / 4, TH->accent);
    tft.drawLine(ox + cs * 3 / 4, oy + cs / 4, ox + cs / 4, oy + cs * 3 / 4, TH->accent);
    tft.drawCircle(ox + cs + cs / 2, oy + cs + cs / 2, cs / 4, TH->danger);
}

static void cover_pong(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    tft.drawFastHLine(x + 4, y + h / 2, w - 8, TH->border);
    tft.fillRoundRect(x + w / 2 - w / 8, y + 6, w / 4, 4, 2, TH->accent_hi);
    tft.fillRoundRect(x + w / 2 - w / 8, y + h - 10, w / 4, 4, 2, TH->accent_hi);
    tft.fillCircle(x + w / 2, y + h / 2, 4, TH->accent_hi);
}

static void cover_breakout(int x, int y, int w, int h, int brick_idx) {
    (void)brick_idx;
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    const int cols = 5;
    const int rows = 3;
    const int bw = (w - 8) / cols;
    const int bh = h / 7 < 6 ? 6 : h / 7;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++)
            tft.fillRoundRect(x + 4 + c * bw, y + 6 + r * (bh + 2), bw - 2, bh, 2,
                              ui_theme_brick_color(r));
    }
    tft.fillRoundRect(x + w / 2 - w / 8, y + h - 14, w / 4, 5, 2, TH->border);
    tft.fillCircle(x + w / 2, y + h / 2 + 6, 4, TH->accent);
}

static void cover_jump(int x, int y, int w, int h, int brick_idx) {
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    tft.fillRoundRect(x + 8, y + h - 14, w / 2, 8, 4, TH->border);
    tft.fillRoundRect(x + w / 3, y + h / 3, w / 3, 8, 4,
                      ui_theme_brick_color(brick_idx + 1));
    tft.fillCircle(x + w / 2, y + h / 2 - 4, 5, ui_theme_brick_color(brick_idx));
}

static void cover_bounce(int x, int y, int w, int h, int brick_idx) {
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    tft.fillRoundRect(x + w / 2 - w / 6, y + h - 12, w / 3, 5, 2, TH->border);
    tft.fillCircle(x + w / 2, y + h / 2 - 6, 5, ui_theme_brick_color(brick_idx + 2));
}

static void cover_stack(int x, int y, int w, int h, int brick_idx) {
    tft.fillRoundRect(x, y, w, h, 4, TH->bg);
    tft.drawRoundRect(x, y, w, h, 4, TH->border);
    for (int i = 0; i < 3; i++) {
        const int bw = w - 8 - i * 6;
        tft.fillRoundRect(x + 4 + i * 3, y + h - 10 - i * 9, bw, 7, 2,
                          ui_theme_brick_color(brick_idx + i));
    }
}

void ui_draw_game_cover(const char* engine, int x, int y, int w, int h, int brick_idx) {
    switch (engine_id(engine)) {
    case ENGINE_SNAKE: cover_snake(x, y, w, h, brick_idx); break;
    case ENGINE_TETRIS: cover_tetris(x, y, w, h, brick_idx); break;
    case ENGINE_PONG: cover_pong(x, y, w, h, brick_idx); break;
    case ENGINE_DODGE: cover_dodge(x, y, w, h, brick_idx); break;
    case ENGINE_SIMON: cover_simon(x, y, w, h, brick_idx); break;
    case ENGINE_MINES: cover_mines(x, y, w, h, brick_idx); break;
    case ENGINE_VELHA: cover_velha(x, y, w, h, brick_idx); break;
    case ENGINE_REACTION: cover_reaction(x, y, w, h, brick_idx); break;
    case ENGINE_RPS: cover_rps(x, y, w, h, brick_idx); break;
    case ENGINE_JUMP: cover_jump(x, y, w, h, brick_idx); break;
    case ENGINE_BOUNCE: cover_bounce(x, y, w, h, brick_idx); break;
    case ENGINE_STACK: cover_stack(x, y, w, h, brick_idx); break;
    default: cover_breakout(x, y, w, h, brick_idx); break;
    }
}

void ui_fill_screen_bg() {
    tft.fillScreen(ui_theme_get()->bg);
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
    if (subtitle && subtitle[0])
        tft.drawString(subtitle, SCREEN_CX, 218, 2);
    else
        tft.drawString("13 jogos", SCREEN_CX, 218, 2);

    tft.fillRoundRect(UI_PAD, 252, UI_CONTENT_W, 4, 2, th->card);
    tft.fillRoundRect(UI_PAD, 252, UI_CONTENT_W * 2 / 3, 4, 2, th->accent);
}
