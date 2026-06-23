#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "audio.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define GW 12
#define GH 16
#define MINES 16
#define FLAG_MS 480
#define COL_BG 0x0000

static const uint16_t COL_COVER = 0xC618;
static const uint16_t COL_OPEN  = 0xDEFB;
static const uint16_t COL_MINE  = 0x0000;
static const uint16_t COL_FLAG  = 0xF800;
static const uint16_t NUM_COL[9] = {
    0x0000, 0x001F, 0x07E0, 0xF800, 0x0010, 0x8000, 0x07FF, 0x0000, 0x3186,
};

static int CELL, OFF_X, OFF_Y;
static int8_t board[GH][GW];
static uint8_t state[GH][GW];
static bool first_tap;
static int revealed, score, flag_count;
static int lives;
static uint32_t start_ms;
static int press_cx, press_cy;
static uint32_t press_ms;
static bool press_pending;
static bool flag_handled;

enum { ST_COVER = 0, ST_OPEN, ST_MINE, ST_FLAG };

static void draw_cover_cell(int px, int py) {
    game_play_fill_rect(px, py, CELL, CELL, COL_COVER);
    game_play_fill_rect(px, py, CELL, 1, 0xFFFF);
    game_play_fill_rect(px, py, 1, CELL, 0xFFFF);
    game_play_fill_rect(px + CELL - 1, py, 1, CELL, 0x4208);
    game_play_fill_rect(px, py + CELL - 1, CELL, 1, 0x4208);
}

static void draw_flag_icon(int px, int py) {
    const int cx = px + CELL / 2;
    const int cy = py + CELL / 2;
    game_play_fill_rect(cx - 1, cy - CELL / 4, 2, CELL / 2, 0xFFFF);
    game_play_fill_rect(cx, cy - CELL / 4, CELL / 3, CELL / 4, COL_FLAG);
    game_play_fill_circle(cx, cy + CELL / 4 - 2, 2, COL_FLAG);
}

static void place_mines(int safe_x, int safe_y) {
    memset(board, 0, sizeof(board));
    int placed = 0;
    while (placed < MINES) {
        const int x = random(0, GW);
        const int y = random(0, GH);
        if (board[y][x] == -1) continue;
        if (x == safe_x && y == safe_y) continue;
        board[y][x] = -1;
        placed++;
    }
    for (int y = 0; y < GH; y++) {
        for (int x = 0; x < GW; x++) {
            if (board[y][x] == -1) continue;
            int n = 0;
            for (int dy = -1; dy <= 1; dy++)
                for (int dx = -1; dx <= 1; dx++) {
                    if (!dx && !dy) continue;
                    const int nx = x + dx, ny = y + dy;
                    if (nx >= 0 && nx < GW && ny >= 0 && ny < GH && board[ny][nx] == -1) n++;
                }
            board[y][x] = (int8_t)n;
        }
    }
}

static void draw_cell(int x, int y) {
    const int px = OFF_X + x * CELL;
    const int py = OFF_Y + y * CELL;

    if (state[y][x] == ST_FLAG) {
        draw_cover_cell(px, py);
        draw_flag_icon(px, py);
        return;
    }
    if (state[y][x] == ST_COVER) {
        draw_cover_cell(px, py);
        return;
    }

    game_play_fill_rect(px, py, CELL, CELL, COL_OPEN);
    if (state[y][x] == ST_MINE) {
        game_play_fill_circle(px + CELL / 2, py + CELL / 2, CELL / 3, COL_MINE);
        return;
    }
    const int v = board[y][x];
    if (v > 0) {
        char s[2] = {(char)('0' + v), 0};
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(NUM_COL[v], COL_OPEN);
        tft.drawString(s, PLAY_X + px + CELL / 2, PLAY_Y + py + CELL / 2, 1);
    }
}

static void draw_mines_hud();

static void mines_redraw() {
    game_play_clear(COL_BG);
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            draw_cell(x, y);
    draw_mines_hud();
}

static void flood(int x, int y) {
    if (x < 0 || x >= GW || y < 0 || y >= GH) return;
    if (state[y][x] != ST_COVER) return;
    state[y][x] = ST_OPEN;
    revealed++;
    draw_cell(x, y);
    if (board[y][x] != 0) return;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++)
            flood(x + dx, y + dy);
}

static void toggle_flag(int x, int y) {
    if (state[y][x] == ST_FLAG) {
        state[y][x] = ST_COVER;
        flag_count--;
        audio_play(SFX_SELECT);
    } else if (state[y][x] == ST_COVER && flag_count < MINES) {
        state[y][x] = ST_FLAG;
        flag_count++;
        audio_play(SFX_FLIP);
    } else {
        return;
    }
    draw_cell(x, y);
}

static void draw_mines_hud() {
    char buf[16];
    snprintf(buf, sizeof(buf), "B:%d/%d", flag_count, MINES);
    tft.setTextDatum(TR_DATUM);
    tft.setTextColor(COL_FLAG, COL_BG);
    tft.drawString(buf, PLAY_X + PLAY_W - 4, PLAY_Y + max(OFF_Y - 8, 2), 1);
}

static int count_neighbor_flags(int x, int y) {
    int n = 0;
    for (int dy = -1; dy <= 1; dy++)
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            const int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < GW && ny >= 0 && ny < GH && state[ny][nx] == ST_FLAG)
                n++;
        }
    return n;
}

static void chord_open(int x, int y, GameHud* hud) {
    if (state[y][x] != ST_OPEN || board[y][x] <= 0) return;
    if (count_neighbor_flags(x, y) != board[y][x]) return;

    bool hit = false;
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            if (!dx && !dy) continue;
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= GW || ny < 0 || ny >= GH) continue;
            if (state[ny][nx] != ST_COVER) continue;
            if (board[ny][nx] == -1) {
                state[ny][nx] = ST_MINE;
                draw_cell(nx, ny);
                hit = true;
            } else {
                flood(nx, ny);
            }
        }
    }
    if (hit) {
        audio_play(SFX_BOMB);
        lives--;
        game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    } else {
        audio_play(SFX_TICK);
    }
    score = revealed * 10;
    game_hud_set_score(hud, score);
}

static void reveal_cell(int x, int y, GameHud* hud) {
    if (state[y][x] != ST_COVER) return;

    if (first_tap) {
        first_tap = false;
        place_mines(x, y);
        start_ms = millis();
    }

    if (board[y][x] == -1) {
        state[y][x] = ST_MINE;
        draw_cell(x, y);
        audio_play(SFX_BOMB);
        lives--;
        game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
        return;
    }
    flood(x, y);
    audio_play(SFX_TICK);
    score = revealed * 10;
    game_hud_set_score(hud, score);
}

static void mines_reset_board() {
    memset(state, 0, sizeof(state));
    memset(board, 0, sizeof(board));
    first_tap = true;
    revealed = 0;
    flag_count = 0;
    press_pending = false;
    start_ms = millis();
    mines_redraw();
}

static void mines_layout() {
    const int cw = PLAY_W / GW;
    const int ch = PLAY_H / GH;
    CELL = cw < ch ? cw : ch;
    if (CELL < 12) CELL = 12;
    OFF_X = (PLAY_W - GW * CELL) / 2;
    OFF_Y = (PLAY_H - GH * CELL) / 2;
    if (OFF_Y < PLAY_MARGIN) OFF_Y = PLAY_MARGIN;
}

static void mines_init(GameHud* hud) {
    mines_layout();
    score = 0;
    lives = GAME_LIVES_DEFAULT;
    mines_reset_board();
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
    game_hud_set_tier_mode(hud, HUD_TIER_NONE, false);
    game_hud_set_score_tag(hud, "Pts");
    game_hud_set_tier(hud, 0);
}

static bool cell_from_play(int px, int py, int* ox, int* oy) {
    const int x = (px - OFF_X) / CELL;
    const int y = (py - OFF_Y) / CELL;
    if (x < 0 || x >= GW || y < 0 || y >= GH) return false;
    *ox = x;
    *oy = y;
    return true;
}

void game_mines_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;
    game_hud_set_score_tag(hud, "Pts");

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        lives = GAME_LIVES_DEFAULT;
        mines_init(hud);
        game_hud_set_score(hud, 0);

        GameInput in;
        bool dead = false;
        bool won = false;

        while (!dead && !won) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                mines_redraw();

            if (in.just_pressed && in.y >= PLAY_Y) {
                int cx, cy;
                if (cell_from_play(in.play_x, in.play_y, &cx, &cy)) {
                    press_cx = cx;
                    press_cy = cy;
                    press_ms = millis();
                    press_pending = true;
                    flag_handled = false;
                }
            }

            if (press_pending && in.down && !flag_handled) {
                int cx, cy;
                if (cell_from_play(in.play_x, in.play_y, &cx, &cy) &&
                    cx == press_cx && cy == press_cy &&
                    millis() - press_ms >= FLAG_MS) {
                    toggle_flag(cx, cy);
                    flag_handled = true;
                }
            }

            if (in.just_released && press_pending) {
                if (!flag_handled) {
                    if (state[press_cy][press_cx] == ST_OPEN && board[press_cy][press_cx] > 0)
                        chord_open(press_cx, press_cy, hud);
                    else
                        reveal_cell(press_cx, press_cy, hud);
                    if (lives <= 0) {
                        dead = true;
                    } else if (state[press_cy][press_cx] == ST_MINE) {
                        mines_reset_board();
                    } else if (revealed >= GW * GH - MINES) {
                        score += (int)((millis() - start_ms) / 100);
                        game_hud_set_score(hud, score);
                        won = true;
                    }
                }
                press_pending = false;
            }

            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, won) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
