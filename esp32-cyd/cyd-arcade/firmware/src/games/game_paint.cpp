#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "ui_theme.h"
#include "ui_draw.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TH ui_theme_get()
#define TOOL_H      52
#define CANVAS_H    (PLAY_H - TOOL_H)
#define MAX_DOTS    3200
#define BRUSH_MIN   1
#define BRUSH_MAX   18
#define MAIN_COLORS 6
#define EXTRA_COLS  4
#define EXTRA_ROWS  3
#define PICKER_BTN_W 32
#define PICKER_HIT_OPEN  105
#define PICKER_HIT_CLOSE 200

typedef struct {
    int16_t x, y;
    uint16_t color;
    int8_t r;
} PaintDot;

/* Primarias + secundarias na barra */
static const uint16_t MAIN_PALETTE[] = {
    0xF800, /* Vermelho */
    0xFFE0, /* Amarelo */
    0x001F, /* Azul */
    0xFD20, /* Laranja */
    0x07E0, /* Verde */
    0x881F, /* Roxo */
};

/* Cores extras — so na janela de selecao */
static const uint16_t EXTRA_PALETTE[] = {
    0x0000, 0xFFFF, 0x8410, 0x8200,
    0xDEFB, 0xFC9F, 0xF81F, 0x07FF,
    0x051F, 0x3666, 0xFEA0, 0xFB66,
};

static PaintDot dots[MAX_DOTS];
static int dot_n;
static int color_idx;
static int extra_idx;
static bool use_extra;
static bool picker_open;
static int brush_r;
static bool erasing;
static int last_x, last_y;
static bool drawing;
static int picker_ox, picker_oy, picker_ow, picker_oh;
static int picker_grid_x, picker_grid_y, picker_cell;

static int extra_n() {
    return (int)(sizeof(EXTRA_PALETTE) / sizeof(EXTRA_PALETTE[0]));
}

static uint16_t active_color() {
    if (erasing) return game_play_field_bg();
    if (use_extra) return EXTRA_PALETTE[extra_idx];
    return MAIN_PALETTE[color_idx];
}

static int clamp_paint_y(int y, int r) {
    if (r <= 1) {
        if (y < 0) return 0;
        if (y >= CANVAS_H) return CANVAS_H - 1;
        return y;
    }
    const int lo = r;
    const int hi = CANVAS_H - r - 1;
    if (hi < lo) return CANVAS_H / 2;
    if (y < lo) return lo;
    if (y > hi) return hi;
    return y;
}

static void paint_stamp(int x, int y, uint16_t col, int r) {
    if (r <= 0) return;
    const int y_bot = CANVAS_H - 1;

    for (int dy = -r; dy <= r; dy++) {
        const int yy = y + dy;
        if (yy < 0 || yy > y_bot) continue;

        const int dx = (int)sqrtf((float)(r * r - dy * dy));
        int x0 = x - dx;
        int w = dx * 2 + 1;
        if (x0 < 0) {
            w += x0;
            x0 = 0;
        }
        if (x0 + w > PLAY_W)
            w = PLAY_W - x0;
        if (w > 0)
            game_play_fill_rect(x0, yy, w, 1, col);
    }
}

static bool stroke_touches_toolbar(int y, int r) {
    return y + r >= CANVAS_H - 2;
}

static bool paint_dot(int x, int y, uint16_t col, int r) {
    if (r <= 0) return false;
    y = clamp_paint_y(y, r);
    if (x < -r || x >= PLAY_W + r || y < -r || y >= CANVAS_H) return false;
    if (dot_n < MAX_DOTS) {
        dots[dot_n].x = (int16_t)x;
        dots[dot_n].y = (int16_t)y;
        dots[dot_n].color = col;
        dots[dot_n].r = (int8_t)r;
        dot_n++;
    }
    paint_stamp(x, y, col, r);
    return stroke_touches_toolbar(y, r);
}

static bool paint_line(int x0, int y0, int x1, int y1, uint16_t col, int r) {
    y0 = clamp_paint_y(y0, r);
    y1 = clamp_paint_y(y1, r);
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    int steps = dx > dy ? dx : dy;
    if (steps < 1) steps = 1;
    bool touch = stroke_touches_toolbar(y0, r) || stroke_touches_toolbar(y1, r);
    for (int i = 0; i <= steps; i++) {
        const int x = x0 + (x1 - x0) * i / steps;
        const int y = y0 + (y1 - y0) * i / steps;
        if (paint_dot(x, y, col, r))
            touch = true;
    }
    return touch;
}

static void draw_color_swatch(int cx, int cy, uint16_t col, bool selected, int r) {
    game_play_fill_circle(cx, cy, r, col);
    if (selected) {
        game_play_fill_circle(cx, cy, r + 2, TH->accent);
        game_play_fill_circle(cx, cy, r, col);
    }
    if (col == 0x0000)
        tft.drawCircle(PLAY_X + cx, PLAY_Y + cy, r, TH->border);
}

static void draw_color_row() {
    const int cy = CANVAS_H + 13;
    const int picker_x = PLAY_W - PICKER_BTN_W - 4;
    const int usable = picker_x - 8;
    const int step = usable / MAIN_COLORS;

    for (int i = 0; i < MAIN_COLORS; i++) {
        const int cx = 4 + step / 2 + i * step;
        const bool sel = !use_extra && i == color_idx;
        draw_color_swatch(cx, cy, MAIN_PALETTE[i], sel, 8);
    }

    const int bx = picker_x;
    const int by = CANVAS_H + 2;
    const uint16_t btn_fill = use_extra ? EXTRA_PALETTE[extra_idx] : TH->card;
    game_play_fill_round_rect(bx, by, PICKER_BTN_W, 22, 5, btn_fill);
    if (use_extra) {
        tft.drawRoundRect(PLAY_X + bx, PLAY_Y + by, PICKER_BTN_W, 22, 5, TH->accent);
        if (EXTRA_PALETTE[extra_idx] == 0x0000)
            tft.drawRoundRect(PLAY_X + bx + 1, PLAY_Y + by + 1, PICKER_BTN_W - 2, 20, 4, TH->border);
    } else {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->text_hi, TH->card);
        tft.drawString("+", PLAY_X + bx + PICKER_BTN_W / 2, PLAY_Y + CANVAS_H + 13, 2);
    }
}

static void draw_size_controls() {
    const int row2 = CANVAS_H + 28;
    const int cy = row2 + 12;

    game_play_fill_round_rect(6, row2 + 4, 28, 24, 5, TH->card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("-", PLAY_X + 20, PLAY_Y + cy, 2);

    char buf[4];
    snprintf(buf, sizeof(buf), "%d", brush_r);
    game_play_fill_round_rect(38, row2 + 4, 28, 24, 5, TH->surface);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(buf, PLAY_X + 52, PLAY_Y + cy, 2);

    game_play_fill_round_rect(70, row2 + 4, 28, 24, 5, TH->card);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("+", PLAY_X + 84, PLAY_Y + cy, 2);

    const int preview_r = brush_r > 12 ? 12 : brush_r;
    if (preview_r > 0) {
        const int py = cy;
        const uint16_t pcol = active_color();
        for (int dy = -preview_r; dy <= preview_r; dy++) {
            const int yy = py + dy;
            if (yy < CANVAS_H) continue;
            const int dx = (int)sqrtf((float)(preview_r * preview_r - dy * dy));
            int x0 = 118 - dx;
            int w = dx * 2 + 1;
            if (x0 < 0) {
                w += x0;
                x0 = 0;
            }
            if (x0 + w > PLAY_W)
                w = PLAY_W - x0;
            if (w > 0)
                game_play_fill_rect(x0, yy, w, 1, pcol);
        }
        if (erasing)
            tft.drawCircle(PLAY_X + 118, PLAY_Y + cy, preview_r, TH->border);
    }

    const int ex = PLAY_W - 44;
    game_play_fill_round_rect(ex, row2 + 4, 36, 24, 6, erasing ? TH->accent : TH->card);
    tft.setTextColor(TH->text_hi, erasing ? TH->accent : TH->card);
    tft.drawString("E", PLAY_X + ex + 18, PLAY_Y + cy, 2);

    game_play_fill_round_rect(PLAY_W - 18, row2 + 6, 14, 20, 4, TH->danger);
    tft.setTextColor(0xFFFF, TH->danger);
    tft.drawString("C", PLAY_X + PLAY_W - 11, PLAY_Y + cy, 1);
}

static void draw_toolbar() {
    game_play_fill_rect(0, CANVAS_H, PLAY_W, TOOL_H, TH->surface);
    tft.drawFastHLine(PLAY_X, PLAY_Y + CANVAS_H, PLAY_W, TH->border);
    tft.drawFastHLine(PLAY_X, PLAY_Y + CANVAS_H + 26, PLAY_W, TH->border);
    draw_color_row();
    draw_size_controls();
}

static void draw_picker_modal() {
    const uint16_t scrim = ui_tint565(TH->bg, -48);
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, scrim);

    picker_ow = PLAY_W - 16;
    picker_oh = 196;
    picker_ox = 8;
    picker_oy = (CANVAS_H - picker_oh) / 2;

    game_play_fill_round_rect(picker_ox, picker_oy, picker_ow, picker_oh, UI_MODAL_R, TH->card);
    tft.drawRoundRect(PLAY_X + picker_ox, PLAY_Y + picker_oy, picker_ow, picker_oh,
                      UI_MODAL_R, TH->border);
    tft.drawRoundRect(PLAY_X + picker_ox + 1, PLAY_Y + picker_oy + 1,
                      picker_ow - 2, picker_oh - 2, UI_MODAL_R - 1, TH->accent);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("Mais cores", PLAY_X + picker_ox + picker_ow / 2, PLAY_Y + picker_oy + 16, 2);

    game_play_fill_round_rect(picker_ox + picker_ow - 30, picker_oy + 4, 26, 22, 5, TH->surface);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString("X", PLAY_X + picker_ox + picker_ow - 17, PLAY_Y + picker_oy + 15, 2);

    picker_grid_y = picker_oy + 34;
    picker_grid_x = picker_ox + 8;
    picker_cell = (picker_ow - 16) / EXTRA_COLS;

    for (int i = 0; i < extra_n(); i++) {
        const int col = i % EXTRA_COLS;
        const int row = i / EXTRA_COLS;
        const int cx = picker_grid_x + col * picker_cell + picker_cell / 2;
        const int cy = picker_grid_y + row * picker_cell + picker_cell / 2;
        const bool sel = use_extra && extra_idx == i;
        draw_color_swatch(cx, cy, EXTRA_PALETTE[i], sel, 14);
    }
}

static int picker_hit(int px, int py) {
    if (py >= picker_oy + 4 && py < picker_oy + 26 &&
        px >= picker_ox + picker_ow - 30 && px < picker_ox + picker_ow)
        return PICKER_HIT_CLOSE;

    if (py < picker_grid_y || py >= picker_grid_y + EXTRA_ROWS * picker_cell)
        return -1;
    if (px < picker_grid_x || px >= picker_grid_x + EXTRA_COLS * picker_cell)
        return -1;

    const int col = (px - picker_grid_x) / picker_cell;
    const int row = (py - picker_grid_y) / picker_cell;
    const int idx = row * EXTRA_COLS + col;
    if (idx < 0 || idx >= extra_n())
        return -1;
    return idx;
}

static void replay_dots() {
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, game_play_field_bg());
    for (int i = 0; i < dot_n; i++)
        paint_stamp(dots[i].x, dots[i].y, dots[i].color, dots[i].r);
    draw_toolbar();
}

static int toolbar_hit(int px, int py) {
    if (py < CANVAS_H) return -1;

    if (py >= CANVAS_H + 26) {
        if (px >= PLAY_W - 22) return 100;
        if (px >= PLAY_W - 48) return 99;
        if (px >= 6 && px < 34) return 101;
        if (px >= 70 && px < 98) return 102;
        return -1;
    }

    const int picker_x = PLAY_W - PICKER_BTN_W - 4;
    if (px >= picker_x && px < picker_x + PICKER_BTN_W)
        return PICKER_HIT_OPEN;

    const int usable = picker_x - 8;
    const int step = usable / MAIN_COLORS;
    const int local = (px - 4) / step;
    if (local >= 0 && local < MAIN_COLORS)
        return local;
    return -1;
}

static void paint_clear() {
    dot_n = 0;
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, game_play_field_bg());
    draw_toolbar();
    buzzer_play(SFX_SELECT);
}

static void brush_resize(int delta) {
    const int next = brush_r + delta;
    if (next < BRUSH_MIN || next > BRUSH_MAX) {
        buzzer_play(SFX_ERROR);
        return;
    }
    brush_r = next;
    draw_toolbar();
    buzzer_play(SFX_TICK);
}

static void open_picker() {
    picker_open = true;
    drawing = false;
    draw_picker_modal();
    buzzer_play(SFX_TICK);
}

static void close_picker() {
    picker_open = false;
    replay_dots();
}

static void paint_init() {
    dot_n = 0;
    color_idx = 0;
    extra_idx = 0;
    use_extra = false;
    picker_open = false;
    brush_r = 5;
    erasing = false;
    drawing = false;
    last_x = last_y = -1;
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, game_play_field_bg());
    draw_toolbar();
}

void game_paint_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_NONE, false);
    game_hud_set_score_visible(hud, false);

    paint_init();

    GameInput in;
    for (;;) {
        game_frame_tick();
        game_input_poll(&in);
        if (game_hud_poll(hud)) {
            game_hud_end(hud);
            return;
        }
        if (game_hud_consume_resume_redraw(hud)) {
            if (picker_open)
                draw_picker_modal();
            else
                replay_dots();
        }

        if (picker_open) {
            if (in.just_pressed) {
                const int hit = picker_hit(in.play_x, in.play_y);
                if (hit == PICKER_HIT_CLOSE) {
                    close_picker();
                    buzzer_play(SFX_SELECT);
                } else if (hit >= 0) {
                    use_extra = true;
                    extra_idx = hit;
                    erasing = false;
                    close_picker();
                    buzzer_play(SFX_SELECT);
                }
            }
            game_frame_delay();
            continue;
        }

        if (in.just_pressed && in.y >= PLAY_Y) {
            const int hit = toolbar_hit(in.play_x, in.play_y);
            if (hit == 100) {
                paint_clear();
            } else if (hit == 101) {
                brush_resize(-1);
            } else if (hit == 102) {
                brush_resize(1);
            } else if (hit == 99) {
                erasing = !erasing;
                draw_toolbar();
                buzzer_play(SFX_SELECT);
            } else if (hit == PICKER_HIT_OPEN) {
                open_picker();
            } else if (hit >= 0 && hit < MAIN_COLORS) {
                color_idx = hit;
                use_extra = false;
                erasing = false;
                draw_toolbar();
                buzzer_play(SFX_TICK);
            } else if (in.play_y < CANVAS_H) {
                drawing = true;
                last_x = in.play_x;
                last_y = in.play_y;
                if (paint_dot(last_x, last_y, active_color(), brush_r))
                    draw_toolbar();
            }
        }

        if (in.down && drawing) {
            if (in.play_y >= CANVAS_H) {
                drawing = false;
                draw_toolbar();
            } else if (in.play_x != last_x || in.play_y != last_y) {
                if (paint_line(last_x, last_y, in.play_x, in.play_y, active_color(), brush_r))
                    draw_toolbar();
                last_x = in.play_x;
                last_y = in.play_y;
            }
        }

        if (in.just_released) {
            if (drawing)
                draw_toolbar();
            drawing = false;
        }

        game_frame_delay();
    }
}
