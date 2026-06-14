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
#define PICKER_BTN_W 32
#define PICKER_HIT_OPEN  105
#define PICKER_HIT_OK    201
#define PICKER_HIT_CLOSE 200
#define PICK_DRAG_NONE 0
#define PICK_DRAG_SV   1
#define PICK_DRAG_HUE  2

typedef struct {
    int16_t x, y;
    uint16_t color;
    int8_t r;
} PaintDot;

static const uint16_t MAIN_PALETTE[] = {
    0xF800, 0xFFE0, 0x001F, 0xFD20, 0x07E0, 0x881F,
};

static PaintDot dots[MAX_DOTS];
static int dot_n;
static int color_idx;
static bool use_custom;
static uint16_t custom_color;
static float pick_h, pick_s, pick_v;
static bool picker_open;
static int pick_drag;
static int brush_r;
static bool erasing;
static int last_x, last_y;
static bool drawing;

static int picker_ox, picker_oy, picker_ow, picker_oh;
static int sv_x, sv_y, sv_w, sv_h;
static int hue_x, hue_y, hue_w, hue_h;
static int ok_x, ok_y, ok_w, ok_h;
static int prev_x, prev_y;
static int cross_px, cross_py;

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static uint16_t hsv_to565(float h, float s, float v) {
    while (h < 0.0f) h += 360.0f;
    while (h >= 360.0f) h -= 360.0f;
    s = clampf(s, 0.0f, 1.0f);
    v = clampf(v, 0.0f, 1.0f);

    const float c = v * s;
    const float hh = h / 60.0f;
    const float x = c * (1.0f - fabsf(fmodf(hh, 2.0f) - 1.0f));
    const float m = v - c;
    float r = 0, g = 0, b = 0;

    if (hh < 1.0f) {
        r = c; g = x;
    } else if (hh < 2.0f) {
        r = x; g = c;
    } else if (hh < 3.0f) {
        g = c; b = x;
    } else if (hh < 4.0f) {
        g = x; b = c;
    } else if (hh < 5.0f) {
        r = x; b = c;
    } else {
        r = c; b = x;
    }

    const uint8_t R = (uint8_t)((r + m) * 255.0f + 0.5f);
    const uint8_t G = (uint8_t)((g + m) * 255.0f + 0.5f);
    const uint8_t B = (uint8_t)((b + m) * 255.0f + 0.5f);
    return (uint16_t)(((R & 0xF8) << 8) | ((G & 0xFC) << 3) | (B >> 3));
}

static void rgb565_to_hsv(uint16_t c, float* h, float* s, float* v) {
    const float r = ((c >> 11) & 0x1F) / 31.0f;
    const float g = ((c >> 5) & 0x3F) / 63.0f;
    const float b = (c & 0x1F) / 31.0f;
    const float maxc = fmaxf(r, fmaxf(g, b));
    const float minc = fminf(r, fminf(g, b));
    const float d = maxc - minc;

    *v = maxc;
    *s = (maxc <= 0.0f) ? 0.0f : d / maxc;

    if (d <= 0.0001f) {
        *h = pick_h;
        return;
    }

    float hh;
    if (maxc == r)
        hh = (g - b) / d + (g < b ? 6.0f : 0.0f);
    else if (maxc == g)
        hh = (b - r) / d + 2.0f;
    else
        hh = (r - g) / d + 4.0f;
    *h = hh * 60.0f;
}

static uint16_t active_color() {
    if (erasing) return game_play_field_bg();
    if (use_custom) return custom_color;
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
        const bool sel = !use_custom && i == color_idx;
        draw_color_swatch(cx, cy, MAIN_PALETTE[i], sel, 8);
    }

    const int bx = picker_x;
    const int by = CANVAS_H + 2;
    const uint16_t btn_fill = use_custom ? custom_color : TH->card;
    game_play_fill_round_rect(bx, by, PICKER_BTN_W, 22, 5, btn_fill);
    if (use_custom) {
        tft.drawRoundRect(PLAY_X + bx, PLAY_Y + by, PICKER_BTN_W, 22, 5, TH->accent);
        if (custom_color == 0x0000)
            tft.drawRoundRect(PLAY_X + bx + 1, PLAY_Y + by + 1, PICKER_BTN_W - 2, 20, 4, TH->border);
    } else {
        game_play_fill_circle(bx + 10, CANVAS_H + 13, 4, 0xF800);
        game_play_fill_circle(bx + 16, CANVAS_H + 13, 4, 0xFFE0);
        game_play_fill_circle(bx + 22, CANVAS_H + 13, 4, 0x07E0);
        game_play_fill_circle(bx + 16, CANVAS_H + 9, 3, 0x001F);
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

static void picker_layout() {
    picker_ow = PLAY_W - 16;
    picker_oh = 218;
    picker_ox = 8;
    picker_oy = (CANVAS_H - picker_oh) / 2;

    sv_x = picker_ox + 10;
    sv_y = picker_oy + 34;
    sv_w = picker_ow - 20;
    sv_h = 118;

    hue_x = sv_x;
    hue_y = sv_y + sv_h + 10;
    hue_w = sv_w;
    hue_h = 16;

    ok_x = picker_ox + 12;
    ok_y = hue_y + hue_h + 12;
    ok_w = 72;
    ok_h = 28;

    prev_x = picker_ox + picker_ow - 34;
    prev_y = ok_y + ok_h / 2;
}

static void picker_cross_pos(int* ox, int* oy) {
    *ox = sv_x + (int)(pick_s * (sv_w - 1));
    *oy = sv_y + (int)((1.0f - pick_v) * (sv_h - 1));
}

static void draw_sv_field() {
    for (int y = 0; y < sv_h; y++) {
        const float v = 1.0f - (float)y / (float)(sv_h - 1);
        for (int x = 0; x < sv_w; x++) {
            const float s = (float)x / (float)(sv_w - 1);
            const uint16_t col = hsv_to565(pick_h, s, v);
            tft.drawPixel(PLAY_X + sv_x + x, PLAY_Y + sv_y + y, col);
        }
    }
    tft.drawRect(PLAY_X + sv_x, PLAY_Y + sv_y, sv_w, sv_h, TH->border);
}

static void draw_hue_bar() {
    for (int x = 0; x < hue_w; x++) {
        const float h = 360.0f * (float)x / (float)(hue_w - 1);
        const uint16_t col = hsv_to565(h, 1.0f, 1.0f);
        tft.drawFastVLine(PLAY_X + hue_x + x, PLAY_Y + hue_y, hue_h, col);
    }
    tft.drawRect(PLAY_X + hue_x, PLAY_Y + hue_y, hue_w, hue_h, TH->border);

    const int hx = hue_x + (int)(pick_h / 360.0f * (hue_w - 1));
    tft.drawFastVLine(PLAY_X + hx, PLAY_Y + hue_y - 1, hue_h + 2, 0xFFFF);
    tft.drawFastVLine(PLAY_X + hx + 1, PLAY_Y + hue_y - 1, hue_h + 2, 0x0000);
}

static void restore_sv_patch(int cx, int cy) {
    const int x0 = cx - 7;
    const int y0 = cy - 7;
    for (int dy = 0; dy <= 14; dy++) {
        const int py = y0 + dy;
        if (py < sv_y || py >= sv_y + sv_h) continue;
        const float v = 1.0f - (float)(py - sv_y) / (float)(sv_h - 1);
        for (int dx = 0; dx <= 14; dx++) {
            const int px = x0 + dx;
            if (px < sv_x || px >= sv_x + sv_w) continue;
            const float s = (float)(px - sv_x) / (float)(sv_w - 1);
            tft.drawPixel(PLAY_X + px, PLAY_Y + py, hsv_to565(pick_h, s, v));
        }
    }
}

static void draw_sv_crosshair() {
    picker_cross_pos(&cross_px, &cross_py);
    tft.drawCircle(PLAY_X + cross_px, PLAY_Y + cross_py, 6, 0xFFFF);
    tft.drawCircle(PLAY_X + cross_px, PLAY_Y + cross_py, 7, 0x0000);
}

static void draw_picker_preview() {
    game_play_fill_circle(prev_x, prev_y, 14, custom_color);
    tft.drawCircle(PLAY_X + prev_x, PLAY_Y + prev_y, 14, TH->border);
    if (custom_color == 0x0000)
        tft.drawCircle(PLAY_X + prev_x, PLAY_Y + prev_y, 13, TH->text_mute);
}

static void draw_picker_chrome() {
    game_play_fill_round_rect(picker_ox, picker_oy, picker_ow, picker_oh, UI_MODAL_R, TH->card);
    tft.drawRoundRect(PLAY_X + picker_ox, PLAY_Y + picker_oy, picker_ow, picker_oh,
                      UI_MODAL_R, TH->border);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    tft.drawString("Cor", PLAY_X + picker_ox + 24, PLAY_Y + picker_oy + 16, 2);

    game_play_fill_round_rect(picker_ox + picker_ow - 30, picker_oy + 4, 26, 22, 5, TH->surface);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString("X", PLAY_X + picker_ox + picker_ow - 17, PLAY_Y + picker_oy + 15, 2);

    game_play_fill_round_rect(ok_x, ok_y, ok_w, ok_h, 6, TH->accent);
    tft.setTextColor(TH->bg, TH->accent);
    tft.drawString("OK", PLAY_X + ok_x + ok_w / 2, PLAY_Y + ok_y + ok_h / 2, 2);
}

static void draw_picker_modal() {
    const uint16_t scrim = ui_tint565(TH->bg, -48);
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, scrim);

    picker_layout();
    draw_picker_chrome();
    draw_sv_field();
    draw_hue_bar();
    draw_sv_crosshair();
    draw_picker_preview();
}

static void picker_update_color() {
    custom_color = hsv_to565(pick_h, pick_s, pick_v);
}

static bool point_in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void picker_touch_sv(int px, int py) {
    pick_s = clampf((float)(px - sv_x) / (float)(sv_w - 1), 0.0f, 1.0f);
    pick_v = clampf(1.0f - (float)(py - sv_y) / (float)(sv_h - 1), 0.0f, 1.0f);
    picker_update_color();
}

static void picker_touch_hue(int px) {
    pick_h = clampf((float)(px - hue_x) / (float)(hue_w - 1) * 360.0f, 0.0f, 360.0f);
    picker_update_color();
}

static int picker_hit(int px, int py) {
    if (point_in_rect(px, py, picker_ox + picker_ow - 30, picker_oy + 4, 26, 22))
        return PICKER_HIT_CLOSE;
    if (point_in_rect(px, py, ok_x, ok_y, ok_w, ok_h))
        return PICKER_HIT_OK;
    if (point_in_rect(px, py, sv_x, sv_y, sv_w, sv_h))
        return PICK_DRAG_SV;
    if (point_in_rect(px, py, hue_x, hue_y, hue_w, hue_h))
        return PICK_DRAG_HUE;
    return PICK_DRAG_NONE;
}

static void picker_drag(int px, int py, int zone) {
    const int old_cx = cross_px;
    const int old_cy = cross_py;

    if (zone == PICK_DRAG_SV) {
        picker_touch_sv(px, py);
        restore_sv_patch(old_cx, old_cy);
        draw_sv_crosshair();
        draw_picker_preview();
    } else if (zone == PICK_DRAG_HUE) {
        picker_touch_hue(px);
        draw_sv_field();
        draw_hue_bar();
        draw_sv_crosshair();
        draw_picker_preview();
    }
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

static void picker_seed_from(uint16_t col) {
    rgb565_to_hsv(col, &pick_h, &pick_s, &pick_v);
    custom_color = hsv_to565(pick_h, pick_s, pick_v);
}

static void open_picker() {
    picker_open = true;
    drawing = false;
    pick_drag = PICK_DRAG_NONE;
    picker_seed_from(use_custom ? custom_color : MAIN_PALETTE[color_idx]);
    draw_picker_modal();
    buzzer_play(SFX_TICK);
}

static void close_picker(bool apply) {
    if (apply) {
        use_custom = true;
        erasing = false;
    }
    picker_open = false;
    pick_drag = PICK_DRAG_NONE;
    replay_dots();
}

static void paint_init() {
    dot_n = 0;
    color_idx = 0;
    use_custom = false;
    custom_color = 0xF800;
    pick_h = 0.0f;
    pick_s = 1.0f;
    pick_v = 1.0f;
    picker_open = false;
    pick_drag = PICK_DRAG_NONE;
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
                    close_picker(false);
                    buzzer_play(SFX_SELECT);
                } else if (hit == PICKER_HIT_OK) {
                    close_picker(true);
                    buzzer_play(SFX_SELECT);
                } else if (hit == PICK_DRAG_SV || hit == PICK_DRAG_HUE) {
                    pick_drag = hit;
                    picker_drag(in.play_x, in.play_y, hit);
                    buzzer_play(SFX_TICK);
                }
            } else if (in.down && pick_drag != PICK_DRAG_NONE) {
                picker_drag(in.play_x, in.play_y, pick_drag);
            }

            if (in.just_released)
                pick_drag = PICK_DRAG_NONE;

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
                use_custom = false;
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
