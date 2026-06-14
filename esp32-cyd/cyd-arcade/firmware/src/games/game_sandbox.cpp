#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define TH ui_theme_get()
#define TOOL_H      52
#define CANVAS_H    (PLAY_H - TOOL_H)
#define VISIBLE     5
#define ARROW_W     28

typedef struct {
    int view_x;
    int view_w;
    int item_w;
    int arrow_rx;
} ScrollLayout;

static ScrollLayout scroll_layout() {
    ScrollLayout L;
    L.view_x = ARROW_W;
    L.arrow_rx = PLAY_W - ARROW_W;
    L.view_w = PLAY_W - ARROW_W * 2;
    L.item_w = L.view_w / VISIBLE;
    return L;
}
#define CS       3
#define GW       (PLAY_W / CS)
#define GH       (CANVAS_H / CS)
#define PHYS_MS  22
#define BRUSH_MIN 1
#define BRUSH_MAX 5

enum : uint8_t {
    CELL_EMPTY = 0,
    CELL_SAND,
    CELL_WATER,
    CELL_STONE,
    CELL_DIRT,
    CELL_MUD,
    CELL_FIRE,
    CELL_STEAM,
    CELL_WOOD,
    CELL_GRASS,
    CELL_OIL,
    CELL_ICE,
    CELL_LAVA,
};

static uint8_t grid[GH][GW];
static uint8_t prev[GH][GW];
static int mat_idx;
static int brush_r;
static int scroll_off;
static bool erasing;
static bool painting;
static int last_px, last_py;
static uint32_t last_phys;
static uint8_t phys_tick;
static uint32_t toast_until_ms;
static int16_t toast_x, toast_y, toast_w, toast_h;

static const struct {
    uint8_t id;
    uint16_t color;
    const char* name;
} MATERIALS[] = {
    {CELL_SAND,  0xFD20, "Areia"},
    {CELL_WATER, 0x051F, "Agua"},
    {CELL_DIRT,  0x8200, "Terra"},
    {CELL_STONE, 0x7BEF, "Pedra"},
    {CELL_WOOD,  0xC010, "Madeira"},
    {CELL_GRASS, 0x0660, "Grama"},
    {CELL_OIL,   0x3060, "Oleo"},
    {CELL_ICE,   0xAFDF, "Gelo"},
    {CELL_LAVA,  0xF980, "Lava"},
    {CELL_FIRE,  0xF800, "Fogo"},
};

static int material_n() {
    return (int)(sizeof(MATERIALS) / sizeof(MATERIALS[0]));
}

static int scroll_max() {
    const int n = material_n();
    return n > VISIBLE ? n - VISIBLE : 0;
}

static void ensure_mat_visible() {
    if (mat_idx < scroll_off)
        scroll_off = mat_idx;
    if (mat_idx >= scroll_off + VISIBLE)
        scroll_off = mat_idx - VISIBLE + 1;
    if (scroll_off > scroll_max())
        scroll_off = scroll_max();
    if (scroll_off < 0)
        scroll_off = 0;
}

static uint16_t cell_color(uint8_t id) {
    switch (id) {
    case CELL_SAND:  return 0xFD20;
    case CELL_WATER: return 0x051F;
    case CELL_STONE: return 0x7BEF;
    case CELL_DIRT:  return 0x8200;
    case CELL_MUD:   return 0x4166;
    case CELL_FIRE:  return (phys_tick & 2) ? 0xF800 : 0xFD20;
    case CELL_STEAM: return (phys_tick & 4) ? 0xC618 : 0xDEFB;
    case CELL_WOOD:  return 0xC010;
    case CELL_GRASS: return (phys_tick & 2) ? 0x0660 : 0x07E0;
    case CELL_OIL:   return 0x3060;
    case CELL_ICE:   return (phys_tick & 2) ? 0xAFDF : 0xC6FF;
    case CELL_LAVA:  return (phys_tick & 2) ? 0xF980 : 0xFB60;
    default:         return game_play_field_bg();
    }
}

static bool in_bounds(int r, int c) {
    return r >= 0 && r < GH && c >= 0 && c < GW;
}

static bool cell_empty(int r, int c) {
    return in_bounds(r, c) && grid[r][c] == CELL_EMPTY;
}

static bool cell_fluid_pass(int r, int c) {
    return in_bounds(r, c) && grid[r][c] == CELL_EMPTY;
}

static void draw_cell(int r, int c) {
    const int x = c * CS;
    const int y = r * CS;
    game_play_fill_rect(x, y, CS, CS, cell_color(grid[r][c]));
}

static void sync_grid() {
    for (int r = 0; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] == prev[r][c]) continue;
            draw_cell(r, c);
            prev[r][c] = grid[r][c];
        }
    }
}

static void draw_arrow_btn(int x, int y, int w, int h, const char* label, bool dim) {
    game_play_fill_round_rect(x, y, w, h, 5, dim ? TH->surface : TH->card);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(dim ? TH->text_mute : TH->text_hi, dim ? TH->surface : TH->card);
    tft.drawString(label, PLAY_X + x + w / 2, PLAY_Y + y + h / 2, 2);
}

static void draw_scroll_row() {
    const ScrollLayout L = scroll_layout();
    const int cy = CANVAS_H + 13;
    const int n = material_n();

    draw_arrow_btn(0, CANVAS_H + 2, ARROW_W, 22, "<", scroll_off <= 0);
    draw_arrow_btn(L.arrow_rx, CANVAS_H + 2, ARROW_W, 22, ">",
                   scroll_off >= scroll_max());

    for (int i = 0; i < VISIBLE; i++) {
        const int idx = scroll_off + i;
        const int cx = L.view_x + i * L.item_w + L.item_w / 2;
        if (idx >= n) continue;
        const uint16_t col565 = MATERIALS[idx].color;
        game_play_fill_circle(cx, cy, 9, col565);
        if (idx == mat_idx && !erasing) {
            game_play_fill_circle(cx, cy, 11, TH->accent);
            game_play_fill_circle(cx, cy, 9, col565);
        }
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

    const uint16_t preview_col = erasing ? game_play_field_bg() : MATERIALS[mat_idx].color;
    const int preview_r = brush_r + 2;
    if (preview_r > 0) {
        for (int dy = -preview_r; dy <= preview_r; dy++) {
            const int yy = cy + dy;
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
                game_play_fill_rect(x0, yy, w, 1, preview_col);
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

static void redraw_rect(int x, int y, int w, int h) {
    const int c0 = x / CS;
    const int c1 = (x + w - 1) / CS;
    const int r0 = y / CS;
    const int r1 = (y + h - 1) / CS;
    for (int r = r0; r <= r1 && r < GH; r++) {
        if (r < 0) continue;
        for (int c = c0; c <= c1 && c < GW; c++) {
            if (c < 0) continue;
            if (grid[r][c] == CELL_EMPTY)
                game_play_fill_rect(c * CS, r * CS, CS, CS, game_play_field_bg());
            else
                draw_cell(r, c);
        }
    }
}

static void show_mat_toast(int idx) {
    if (idx < 0 || idx >= material_n()) return;

    if (toast_until_ms)
        redraw_rect(toast_x, toast_y, toast_w, toast_h);

    const char* label = MATERIALS[idx].name;
    const uint16_t col565 = MATERIALS[idx].color;
    const int pad = 8;
    const int th = 22;

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, TH->card);
    const int tw = tft.textWidth(label, 2);
    toast_w = tw + pad * 2 + 18;
    if (toast_w > PLAY_W - 8)
        toast_w = PLAY_W - 8;
    toast_h = th;
    toast_x = (PLAY_W - toast_w) / 2;
    toast_y = 4;

    game_play_fill_round_rect(toast_x, toast_y, toast_w, toast_h, 6, TH->card);
    game_play_fill_circle(toast_x + 12, toast_y + th / 2, 5, col565);
    tft.drawString(label, PLAY_X + toast_x + toast_w / 2 + 4, PLAY_Y + toast_y + th / 2, 2);

    toast_until_ms = millis() + 1400;
}

static void toast_tick() {
    if (!toast_until_ms || millis() < toast_until_ms) return;
    redraw_rect(toast_x, toast_y, toast_w, toast_h);
    toast_until_ms = 0;
}

static void draw_toolbar() {
    game_play_fill_rect(0, CANVAS_H, PLAY_W, TOOL_H, TH->surface);
    tft.drawFastHLine(PLAY_X, PLAY_Y + CANVAS_H, PLAY_W, TH->border);
    tft.drawFastHLine(PLAY_X, PLAY_Y + CANVAS_H + 26, PLAY_W, TH->border);
    draw_scroll_row();
    draw_size_controls();
}

static void sandbox_redraw() {
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, game_play_field_bg());
    for (int r = 0; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] == CELL_EMPTY) continue;
            draw_cell(r, c);
        }
    }
    memcpy(prev, grid, sizeof(prev));
    draw_toolbar();
}

static int toolbar_hit(int px, int py) {
    if (py < CANVAS_H) return -1;
    const ScrollLayout L = scroll_layout();

    if (py >= CANVAS_H + 26) {
        if (px >= PLAY_W - 22) return 100;
        if (px >= PLAY_W - 48) return 99;
        if (px >= 6 && px < 34) return 101;
        if (px >= 70 && px < 98) return 102;
        return -1;
    }

    if (px < ARROW_W) return 103;
    if (px >= L.arrow_rx) return 104;

    const int local = (px - L.view_x) / L.item_w;
    if (local >= 0 && local < VISIBLE) {
        const int idx = scroll_off + local;
        if (idx >= 0 && idx < material_n())
            return idx;
    }
    return -1;
}

static void sandbox_clear() {
    memset(grid, 0, sizeof(grid));
    memset(prev, 0, sizeof(prev));
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, game_play_field_bg());
    draw_toolbar();
    buzzer_play(SFX_SELECT);
}

static void scroll_materials(int delta) {
    const int next = scroll_off + delta;
    if (next < 0 || next > scroll_max()) {
        buzzer_play(SFX_ERROR);
        return;
    }
    scroll_off = next;
    draw_toolbar();
    buzzer_play(SFX_TICK);
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

static void spawn_at(int px, int py) {
    const int cx = px / CS;
    const int cy = py / CS;
    const uint8_t mat = erasing ? CELL_EMPTY : MATERIALS[mat_idx].id;
    bool placed = false;

    for (int dr = -brush_r; dr <= brush_r; dr++) {
        for (int dc = -brush_r; dc <= brush_r; dc++) {
            if (dr * dr + dc * dc > brush_r * brush_r + 1) continue;
            const int r = cy + dr;
            const int c = cx + dc;
            if (!in_bounds(r, c)) continue;
            if (grid[r][c] == mat) continue;
            grid[r][c] = mat;
            placed = true;
        }
    }

    if (placed)
        sync_grid();
}

static void spawn_line(int x0, int y0, int x1, int y1) {
    const int dx = abs(x1 - x0);
    const int dy = abs(y1 - y0);
    int steps = dx > dy ? dx : dy;
    if (steps < 1) steps = 1;
    for (int i = 0; i <= steps; i++) {
        const int x = x0 + (x1 - x0) * i / steps;
        const int y = y0 + (y1 - y0) * i / steps;
        spawn_at(x, y);
    }
}

static void step_reactions() {
    for (int r = 0; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            const uint8_t t = grid[r][c];
            if (t != CELL_WATER) continue;

            static const int dr[] = {1, 0, 0, -1};
            static const int dc[] = {0, 1, -1, 0};
            for (int i = 0; i < 4; i++) {
                const int nr = r + dr[i];
                const int nc = c + dc[i];
                if (!in_bounds(nr, nc)) continue;
                const uint8_t n = grid[nr][nc];
                if (n == CELL_SAND || n == CELL_DIRT || n == CELL_GRASS) {
                    grid[nr][nc] = CELL_MUD;
                    grid[r][c] = CELL_EMPTY;
                    break;
                }
                if (n == CELL_LAVA) {
                    grid[nr][nc] = CELL_STONE;
                    grid[r][c] = CELL_STEAM;
                    break;
                }
            }
        }
    }

    for (int r = 0; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != CELL_LAVA) continue;
            static const int dr[] = {1, 0, 0, -1};
            static const int dc[] = {0, 1, -1, 0};
            for (int i = 0; i < 4; i++) {
                const int nr = r + dr[i];
                const int nc = c + dc[i];
                if (!in_bounds(nr, nc)) continue;
                if (grid[nr][nc] == CELL_WATER) {
                    grid[nr][nc] = CELL_STEAM;
                    grid[r][c] = CELL_STONE;
                    break;
                }
            }
        }
    }

    for (int r = 0; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            const uint8_t t = grid[r][c];
            if (t != CELL_ICE && t != CELL_WOOD && t != CELL_GRASS && t != CELL_OIL) continue;

            static const int dr[] = {-1, 1, 0, 0};
            static const int dc[] = {0, 0, -1, 1};
            bool hot = false;
            for (int i = 0; i < 4; i++) {
                const int nr = r + dr[i];
                const int nc = c + dc[i];
                if (!in_bounds(nr, nc)) continue;
                if (grid[nr][nc] == CELL_FIRE || grid[nr][nc] == CELL_LAVA) {
                    hot = true;
                    break;
                }
            }
            if (!hot) continue;

            if (t == CELL_ICE)
                grid[r][c] = CELL_WATER;
            else
                grid[r][c] = CELL_FIRE;
        }
    }

    for (int r = 0; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != CELL_FIRE) continue;

            static const int dr[] = {-1, 1, 0, 0};
            static const int dc[] = {0, 0, -1, 1};
            for (int i = 0; i < 4; i++) {
                const int nr = r + dr[i];
                const int nc = c + dc[i];
                if (!in_bounds(nr, nc)) continue;
                const uint8_t n = grid[nr][nc];
                if (n == CELL_WATER) {
                    grid[nr][nc] = CELL_STEAM;
                    grid[r][c] = CELL_EMPTY;
                    break;
                }
                if (n == CELL_SAND || n == CELL_DIRT || n == CELL_MUD ||
                    n == CELL_WOOD || n == CELL_GRASS || n == CELL_OIL) {
                    int chance = 40;
                    if (n == CELL_OIL || n == CELL_GRASS)
                        chance = 58;
                    if (random(0, 100) < chance)
                        grid[nr][nc] = CELL_FIRE;
                    else if (random(0, 100) < 50)
                        grid[nr][nc] = CELL_STEAM;
                    else
                        grid[nr][nc] = CELL_EMPTY;
                }
            }

            if (grid[r][c] == CELL_FIRE && random(0, 100) < 8)
                grid[r][c] = CELL_EMPTY;
        }
    }
}

static void step_fire() {
    for (int r = 1; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != CELL_FIRE) continue;

            if (cell_empty(r - 1, c)) {
                grid[r][c] = CELL_EMPTY;
                grid[r - 1][c] = CELL_FIRE;
            } else if (grid[r - 1][c] == CELL_WATER) {
                grid[r - 1][c] = CELL_STEAM;
                grid[r][c] = CELL_EMPTY;
            } else {
                const int dir = random(0, 2) ? -1 : 1;
                if (cell_empty(r - 1, c + dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r - 1][c + dir] = CELL_FIRE;
                } else if (cell_empty(r, c + dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r][c + dir] = CELL_FIRE;
                } else if (cell_empty(r, c - dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r][c - dir] = CELL_FIRE;
                }
            }
        }
    }
}

static void step_steam() {
    for (int r = 1; r < GH; r++) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != CELL_STEAM) continue;

            if (r <= 1 && random(0, 100) < 10) {
                grid[r][c] = CELL_WATER;
                continue;
            }

            if (random(0, 100) < 18) {
                grid[r][c] = CELL_EMPTY;
                continue;
            }

            if (cell_empty(r - 1, c)) {
                grid[r][c] = CELL_EMPTY;
                grid[r - 1][c] = CELL_STEAM;
            } else {
                const int dir = random(0, 2) ? -1 : 1;
                if (cell_empty(r, c + dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r][c + dir] = CELL_STEAM;
                } else if (cell_empty(r - 1, c + dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r - 1][c + dir] = CELL_STEAM;
                }
            }
        }
    }
}

static bool grain_can_sink(int r, int c, uint8_t t) {
    if (!in_bounds(r, c)) return false;
    const uint8_t n = grid[r][c];
    if (n == CELL_EMPTY) return true;
    if (n == CELL_WATER && t != CELL_MUD && t != CELL_LAVA) return true;
    return false;
}

static bool grain_moves_this_tick(uint8_t t) {
    if (t == CELL_SAND || t == CELL_DIRT) return true;
    if (t == CELL_GRASS) return (phys_tick & 1) == 0;
    if (t == CELL_MUD) return (phys_tick % 3) == 0;
    if (t == CELL_LAVA) return (phys_tick & 1) == 0;
    return false;
}

static void grain_move_to(int r, int c, int nr, int nc, uint8_t t) {
    if (grid[nr][nc] == CELL_WATER) {
        grid[r][c] = CELL_WATER;
        grid[nr][nc] = t;
    } else {
        grid[r][c] = CELL_EMPTY;
        grid[nr][nc] = t;
    }
}

static bool grain_try_fall(int r, int c, uint8_t t, int dir) {
    if (grain_can_sink(r + 1, c, t)) {
        grain_move_to(r, c, r + 1, c, t);
        return true;
    }
    if (grain_can_sink(r + 1, c + dir, t)) {
        grain_move_to(r, c, r + 1, c + dir, t);
        return true;
    }
    if (grain_can_sink(r + 1, c - dir, t)) {
        grain_move_to(r, c, r + 1, c - dir, t);
        return true;
    }
    return false;
}

static void step_grains_pass(bool rtl) {
    for (int r = GH - 2; r >= 0; r--) {
        const int c0 = rtl ? GW - 1 : 0;
        const int c1 = rtl ? -1 : GW;
        const int cs = rtl ? -1 : 1;
        for (int c = c0; c != c1; c += cs) {
            const uint8_t t = grid[r][c];
            if (t != CELL_SAND && t != CELL_DIRT && t != CELL_MUD &&
                t != CELL_LAVA && t != CELL_GRASS)
                continue;
            if (!grain_moves_this_tick(t)) continue;
            const int dir = ((r + c + phys_tick) & 1) ? 1 : -1;
            grain_try_fall(r, c, t, dir);
        }
    }
}

static void step_grains() {
    step_grains_pass(false);
    step_grains_pass(true);
}

static void step_buoyancy() {
    for (int r = GH - 2; r >= 0; r--) {
        for (int c = 0; c < GW; c++) {
            if (grid[r][c] != CELL_WATER || grid[r + 1][c] != CELL_OIL) continue;
            grid[r][c] = CELL_OIL;
            grid[r + 1][c] = CELL_WATER;
        }
    }
}

static void step_fluid_pass(uint8_t fluid, bool rtl) {
    for (int r = GH - 2; r >= 0; r--) {
        const int c0 = rtl ? GW - 1 : 0;
        const int c1 = rtl ? -1 : GW;
        const int cs = rtl ? -1 : 1;
        for (int c = c0; c != c1; c += cs) {
            if (grid[r][c] != fluid) continue;

            if (cell_fluid_pass(r + 1, c)) {
                grid[r][c] = CELL_EMPTY;
                grid[r + 1][c] = fluid;
            } else {
                const int dir = ((r + c + phys_tick) & 1) ? 1 : -1;
                if (cell_fluid_pass(r, c + dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r][c + dir] = fluid;
                } else if (cell_fluid_pass(r, c - dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r][c - dir] = fluid;
                } else if (cell_fluid_pass(r + 1, c + dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r + 1][c + dir] = fluid;
                } else if (cell_fluid_pass(r + 1, c - dir)) {
                    grid[r][c] = CELL_EMPTY;
                    grid[r + 1][c - dir] = fluid;
                }
            }
        }
    }
}

static void step_water() {
    step_fluid_pass(CELL_WATER, false);
    step_fluid_pass(CELL_WATER, true);
}

static void step_oil() {
    step_fluid_pass(CELL_OIL, false);
    step_fluid_pass(CELL_OIL, true);
}

static void physics_step() {
    phys_tick++;
    step_reactions();
    step_fire();
    step_steam();
    step_grains();
    step_buoyancy();
    step_water();
    step_oil();
    /* segundo passe de grãos — desbloqueia pilhas que ficaram presas */
    step_grains();
    sync_grid();
}

static void sandbox_init() {
    memset(grid, 0, sizeof(grid));
    memset(prev, 0, sizeof(prev));
    mat_idx = 0;
    brush_r = 2;
    scroll_off = 0;
    erasing = false;
    painting = false;
    phys_tick = 0;
    toast_until_ms = 0;
    last_px = last_py = -1;
    last_phys = millis();
    ensure_mat_visible();
    game_play_fill_rect(0, 0, PLAY_W, CANVAS_H, game_play_field_bg());
    draw_toolbar();
}

void game_sandbox_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_tier_mode(hud, HUD_TIER_NONE, false);
    game_hud_set_score_visible(hud, false);

    sandbox_init();

    GameInput in;
    for (;;) {
        game_frame_tick();
        game_input_poll(&in);
        if (game_hud_poll(hud)) {
            game_hud_end(hud);
            return;
        }
        if (game_hud_consume_resume_redraw(hud))
            sandbox_redraw();

        if (in.just_pressed && in.y >= PLAY_Y) {
            const int hit = toolbar_hit(in.play_x, in.play_y);
            if (hit == 100) {
                sandbox_clear();
            } else if (hit == 101) {
                brush_resize(-1);
            } else if (hit == 102) {
                brush_resize(1);
            } else if (hit == 103) {
                scroll_materials(-1);
            } else if (hit == 104) {
                scroll_materials(1);
            } else if (hit == 99) {
                erasing = !erasing;
                draw_toolbar();
                buzzer_play(SFX_SELECT);
            } else if (hit >= 0) {
                mat_idx = hit;
                erasing = false;
                ensure_mat_visible();
                draw_toolbar();
                show_mat_toast(hit);
                buzzer_play(SFX_TICK);
            } else if (in.play_y < CANVAS_H) {
                painting = true;
                last_px = in.play_x;
                last_py = in.play_y;
                spawn_at(last_px, last_py);
            }
        }

        if (in.down && painting && in.play_y < CANVAS_H) {
            if (in.play_x != last_px || in.play_y != last_py) {
                spawn_line(last_px, last_py, in.play_x, in.play_y);
                last_px = in.play_x;
                last_py = in.play_y;
            }
        }

        if (in.just_released)
            painting = false;

        if (millis() - last_phys >= PHYS_MS) {
            last_phys = millis();
            physics_step();
        }

        toast_tick();

        game_frame_delay();
    }
}
