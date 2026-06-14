#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include "ui_draw.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>

#define BW TETRIS_BW
#define BH TETRIS_BH
#define CELL_W TETRIS_CELL_W
#define CELL_H TETRIS_CELL_H
#define BOARD_W TETRIS_BOARD_W
#define BOARD_H TETRIS_BOARD_H
#define OFF_X TETRIS_OFF_X
#define OFF_Y TETRIS_OFF_Y

#define TH ui_theme_get()
#define COL_PLAY  0x0000
#define COL_FRAME 0x4A49
#define COL_BOARD 0x1084
#define COL_PANEL 0x2949

static uint8_t grid[BH][BW];
static int cur_x, cur_y, cur_type, cur_rot;
static int next_type;
static int score, lines, level;
static int lives;
static uint32_t drop_ms, last_drop;

/* Tetris Guideline: I,O,T,S,Z,J,L */
static const uint16_t PIECE_COL[7] = {
    0x07FF, 0xFFE0, 0x9813, 0x07E0, 0xF800, 0x001F, 0xFD20,
};

static const int8_t k_shape[7][4][4][2] = {
    {{{0,1},{1,1},{2,1},{3,1}}, {{2,0},{2,1},{2,2},{2,3}},
     {{0,2},{1,2},{2,2},{3,2}}, {{1,0},{1,1},{1,2},{1,3}}},
    {{{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}},
     {{0,0},{1,0},{0,1},{1,1}}, {{0,0},{1,0},{0,1},{1,1}}},
    {{{1,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{2,1},{1,2}},
     {{0,1},{1,1},{2,1},{1,2}}, {{1,0},{0,1},{1,1},{1,2}}},
    {{{1,0},{2,0},{0,1},{1,1}}, {{1,0},{1,1},{2,1},{2,2}},
     {{1,1},{2,1},{0,2},{1,2}}, {{0,0},{0,1},{1,1},{1,2}}},
    {{{0,0},{1,0},{1,1},{2,1}}, {{2,0},{1,1},{2,1},{1,2}},
     {{0,1},{1,1},{1,2},{2,2}}, {{1,0},{0,1},{1,1},{0,2}}},
    {{{0,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{1,1},{1,2}},
     {{0,1},{1,1},{2,1},{2,2}}, {{1,0},{1,1},{0,2},{1,2}}},
    {{{2,0},{0,1},{1,1},{2,1}}, {{1,0},{1,1},{1,2},{2,2}},
     {{0,1},{1,1},{2,1},{0,2}}, {{0,0},{1,0},{1,1},{1,2}}},
};

static uint16_t cell_color(uint8_t t) {
    if (t == 0) return COL_BOARD;
    return PIECE_COL[t - 1];
}

static void piece_cells(int type, int rot, int px, int py, int out[4][2]) {
    for (int i = 0; i < 4; i++) {
        out[i][0] = px + k_shape[type][rot][i][0];
        out[i][1] = py + k_shape[type][rot][i][1];
    }
}

static bool blocked(int type, int rot, int px, int py) {
    int cells[4][2];
    piece_cells(type, rot, px, py, cells);
    for (int i = 0; i < 4; i++) {
        const int x = cells[i][0];
        const int y = cells[i][1];
        if (x < 0 || x >= BW || y >= BH) return true;
        if (y >= 0 && grid[y][x]) return true;
    }
    return false;
}

static uint16_t cell_bg_at(int y) {
    return (y >= 0 && y < BH) ? COL_BOARD : COL_PLAY;
}

static void clear_cell(int x, int y) {
    if (x < 0 || x >= BW) return;
    const int px = OFF_X + x * CELL_W;
    const int py = OFF_Y + y * CELL_H;
    game_play_fill_rect(px, py, CELL_W, CELL_H, cell_bg_at(y));
}

static void draw_tetris_cell(int gx, int gy, uint16_t col) {
    const int px = OFF_X + gx * CELL_W + 1;
    const int py = OFF_Y + gy * CELL_H + 1;
    const int bw = CELL_W - 2;
    const int bh = CELL_H - 2;
    game_play_fill_rect(px, py, bw, bh, col);
    game_play_fill_rect(px, py, bw, 1, ui_tint565(col, 30));
    game_play_fill_rect(px, py, 1, bh, ui_tint565(col, 30));
}

static void draw_mini_cell(int px, int py, int mw, int mh, uint16_t col) {
    game_play_fill_rect(px, py, mw - 1, mh - 1, col);
}

static void draw_block(int x, int y, uint16_t col) {
    if (x < 0 || x >= BW || y >= BH) return;
    draw_tetris_cell(x, y, col);
}

static void draw_cell_bg(int x, int y) {
    if (x < 0 || x >= BW || y < 0 || y >= BH) return;
    game_play_fill_rect(OFF_X + x * CELL_W, OFF_Y + y * CELL_H, CELL_W, CELL_H, COL_BOARD);
}

static void draw_next_preview() {
    const int px = TETRIS_PROX_X;
    const int py = OFF_Y;
    const int pw = TETRIS_PROX_W;
    const int ph = 108;
    game_play_fill_round_rect(px, py, pw, ph, 4, COL_PANEL);
    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TH->accent, COL_PANEL);
    tft.drawString("Prox", PLAY_X + px + pw / 2, PLAY_Y + py + 8, 1);
    if (next_type < 0) return;

    const int cs = 6;
    const uint16_t col = cell_color((uint8_t)(next_type + 1));
    int cells[4][2];
    piece_cells(next_type, 0, 0, 0, cells);
    int minx = 4, miny = 4, maxx = 0, maxy = 0;
    for (int i = 0; i < 4; i++) {
        if (cells[i][0] < minx) minx = cells[i][0];
        if (cells[i][1] < miny) miny = cells[i][1];
        if (cells[i][0] > maxx) maxx = cells[i][0];
        if (cells[i][1] > maxy) maxy = cells[i][1];
    }
    const int bw = (maxx - minx + 1) * cs;
    const int sx = px + (pw - bw) / 2;
    const int sy = py + 22;
    game_play_fill_rect(px + 1, py + 18, pw - 2, ph - 20, COL_PANEL);
    for (int i = 0; i < 4; i++) {
        const int bx = sx + (cells[i][0] - minx) * cs;
        const int by = sy + (cells[i][1] - miny) * cs;
        draw_mini_cell(bx, by, cs, cs, col);
    }
}

static void flash_rows(const int* rows, int count) {
    if (count <= 0) return;
    for (int i = 0; i < count; i++)
        game_play_fill_rect(OFF_X, OFF_Y + rows[i] * CELL_H, BOARD_W, CELL_H,
                            ui_rgb565(0xFFFFFF));
    delay(80);
}

static void draw_board() {
    game_play_clear(COL_PLAY);
    game_play_fill_rect(OFF_X - 2, OFF_Y - 2, BOARD_W + 4, BOARD_H + 4, COL_FRAME);
    game_play_fill_rect(OFF_X, OFF_Y, BOARD_W, BOARD_H, COL_BOARD);
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            if (grid[y][x]) draw_block(x, y, cell_color(grid[y][x]));
    draw_next_preview();
}

static void draw_piece(int type, int rot, int px, int py) {
    int cells[4][2];
    piece_cells(type, rot, px, py, cells);
    const uint16_t col = cell_color((uint8_t)(type + 1));
    for (int i = 0; i < 4; i++)
        draw_block(cells[i][0], cells[i][1], col);
}

static void restore_cell(int x, int y) {
    if (y >= 0 && y < BH && x >= 0 && x < BW && grid[y][x])
        draw_block(x, y, cell_color(grid[y][x]));
    else
        clear_cell(x, y);
}

static void erase_cells(int type, int rot, int px, int py) {
    int cells[4][2];
    piece_cells(type, rot, px, py, cells);
    for (int i = 0; i < 4; i++)
        restore_cell(cells[i][0], cells[i][1]);
}

static int ghost_at(int type, int rot, int px, int py) {
    int gy = py;
    while (!blocked(type, rot, px, gy + 1))
        gy++;
    return gy;
}

static void erase_floating_piece() {
    const int gy = ghost_at(cur_type, cur_rot, cur_x, cur_y);
    if (gy != cur_y)
        erase_cells(cur_type, cur_rot, cur_x, gy);
    erase_cells(cur_type, cur_rot, cur_x, cur_y);
}

static void draw_ghost_cell(int gx, int gy, uint16_t col) {
    if (gx < 0 || gx >= BW || gy < 0 || gy >= BH) return;
    const int px = OFF_X + gx * CELL_W + 1;
    const int py = OFF_Y + gy * CELL_H + 1;
    const int bw = CELL_W - 2;
    const int bh = CELL_H - 2;
    game_play_fill_rect(px, py, bw, 1, col);
    game_play_fill_rect(px, py + bh - 1, bw, 1, col);
    game_play_fill_rect(px, py, 1, bh, col);
    game_play_fill_rect(px + bw - 1, py, 1, bh, col);
}

static void draw_ghost(int type, int rot, int px, int py) {
    if (py < 0 || py == cur_y) return;
    int cells[4][2];
    piece_cells(type, rot, px, py, cells);
    const uint16_t col = ui_tint565(cell_color((uint8_t)(type + 1)), -20);
    for (int i = 0; i < 4; i++)
        draw_ghost_cell(cells[i][0], cells[i][1], col);
}

static void draw_active_piece() {
    const int gy = ghost_at(cur_type, cur_rot, cur_x, cur_y);
    draw_ghost(cur_type, cur_rot, cur_x, gy);
    draw_piece(cur_type, cur_rot, cur_x, cur_y);
}

static void tetris_redraw_play() {
    draw_board();
    draw_active_piece();
}

static int clear_lines() {
    int cleared = 0;
    int full_rows[BH];

    for (int y = BH - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BW; x++)
            if (!grid[y][x]) full = false;
        if (!full) continue;
        full_rows[cleared++] = y;
    }
    if (cleared == 0) return 0;

    flash_rows(full_rows, cleared);

    for (int y = BH - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < BW; x++)
            if (!grid[y][x]) full = false;
        if (!full) continue;
        for (int yy = y; yy > 0; yy--)
            for (int x = 0; x < BW; x++)
                grid[yy][x] = grid[yy - 1][x];
        for (int x = 0; x < BW; x++) grid[0][x] = 0;
        y++;
    }
    return cleared;
}

static void update_level(GameHud* hud) {
    const int nl = lines / 12 + 1;
    if (nl == level) return;
    if (game_hud_advance_tier(hud, nl))
        tetris_redraw_play();
    level = nl;
    if (drop_ms > 180) drop_ms -= 25;
}

static void lock_piece(GameHud* hud) {
    int cells[4][2];
    piece_cells(cur_type, cur_rot, cur_x, cur_y, cells);
    const uint8_t id = (uint8_t)(cur_type + 1);
    for (int i = 0; i < 4; i++) {
        const int x = cells[i][0];
        const int y = cells[i][1];
        if (y >= 0 && y < BH && x >= 0 && x < BW)
            grid[y][x] = id;
    }
    const int n = clear_lines();
    if (n > 0) {
        static const int pts[] = {0, 100, 300, 500, 800};
        buzzer_play(SFX_LINE);
        score += pts[n < 5 ? n : 4];
        lines += n;
        update_level(hud);
        if (drop_ms > 180) drop_ms -= (uint32_t)n * 12;
    } else {
        buzzer_play(SFX_DROP);
    }
}

static bool spawn_piece() {
    cur_type = next_type >= 0 ? next_type : random(0, 7);
    next_type = random(0, 7);
    cur_rot = 0;
    cur_x = 3;
    cur_y = 0;
    if (blocked(cur_type, cur_rot, cur_x, cur_y)) return false;
    draw_active_piece();
    draw_next_preview();
    return true;
}

static bool move_piece(int dx, int dy) {
    if (!blocked(cur_type, cur_rot, cur_x + dx, cur_y + dy)) {
        erase_floating_piece();
        cur_x += dx;
        cur_y += dy;
        draw_active_piece();
        return true;
    }
    return false;
}

static bool rotate_piece() {
    const int nr = (cur_rot + 1) & 3;
    const int kicks[] = {0, -1, 1, -2, 2};
    for (int i = 0; i < 5; i++) {
        const int tx = cur_x + kicks[i];
        if (blocked(cur_type, nr, tx, cur_y)) continue;
        erase_floating_piece();
        cur_x = tx;
        cur_rot = nr;
        draw_active_piece();
        return true;
    }
    return false;
}

static bool tetris_lose_life(GameHud* hud) {
    lives--;
    buzzer_play(SFX_ERROR);
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    if (lives <= 0) return false;
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            grid[y][x] = 0;
    draw_board();
    return spawn_piece();
}

static bool step_down(GameHud* hud) {
    if (move_piece(0, 1)) {
        score += 1;
        return true;
    }
    lock_piece(hud);
    draw_board();
    if (!spawn_piece()) return false;
    return true;
}

static bool hard_drop(GameHud* hud) {
    erase_floating_piece();
    const int dist = ghost_at(cur_type, cur_rot, cur_x, cur_y) - cur_y;
    cur_y += dist;
    score += dist * 2;
    lock_piece(hud);
    draw_board();
    if (!spawn_piece()) return false;
    return true;
}

static bool handle_input(GameHud* hud, GameInput* in, GameDrag* drag, uint32_t* last_soft) {
    if (in->just_pressed && in->y >= PLAY_Y)
        game_drag_begin(drag, in);

    if (drag->active && in->down) {
        game_drag_update(drag, in);
        const int sh = game_drag_step_h(drag, in, 18);
        if (sh > 0) move_piece(1, 0);
        else if (sh < 0) move_piece(-1, 0);

        const int sv = game_drag_step_v(drag, in, 22);
        if (sv < 0) {
            if (rotate_piece()) buzzer_play(SFX_SELECT);
        } else if (sv > 0 && millis() - *last_soft > 110) {
            *last_soft = millis();
            if (!step_down(hud)) return false;
            game_hud_set_score(hud, score);
        }
    }

    if (in->just_released && drag->active) {
        const int sv = game_drag_swipe_v(drag);
        if (sv > 0 && drag->total_dy > 72) {
            if (!hard_drop(hud))
                return false;
            game_hud_set_score(hud, score);
        }
        drag->active = false;
    }
    return true;
}

static void tetris_init(const GameEntry* cfg, GameHud* hud) {
    for (int y = 0; y < BH; y++)
        for (int x = 0; x < BW; x++)
            grid[y][x] = 0;

    score = 0;
    lines = 0;
    level = 1;
    lives = GAME_LIVES_DEFAULT;
    next_type = random(0, 7);
    drop_ms = cfg->speed > 0 ? cfg->speed : 600;
    last_drop = millis();
    game_hud_set_score(hud, 0);
    game_hud_set_tier(hud, level);
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    draw_board();
}

void game_tetris_run(const GameEntry* cfg) {
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        tetris_init(cfg, hud);

        if (!spawn_piece()) {
            if (game_hud_end_game(hud, 0, false) == GAME_END_MENU) break;
            continue;
        }

        GameInput in;
        GameDrag drag = {};
        uint32_t last_soft = 0;
        bool dead = false;

        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                tetris_redraw_play();
            if (!handle_input(hud, &in, &drag, &last_soft)) {
                if (!tetris_lose_life(hud)) {
                    dead = true;
                    break;
                }
                continue;
            }

            if (millis() - last_drop >= drop_ms) {
                last_drop = millis();
                if (!step_down(hud)) {
                    if (!tetris_lose_life(hud)) {
                        dead = true;
                        break;
                    }
                }
                game_hud_set_score(hud, score);
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, false) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
