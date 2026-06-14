#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>
#include <string.h>

#define GW 12
#define GH 16
#define MINES 16
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
static int revealed, score;
static int lives;
static uint32_t start_ms;

enum { ST_COVER = 0, ST_OPEN, ST_MINE, ST_FLAG };

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
    if (state[y][x] == ST_COVER) {
        game_play_fill_rect(px, py, CELL, CELL, COL_COVER);
        game_play_fill_rect(px, py, CELL, 1, 0xFFFF);
        game_play_fill_rect(px, py, 1, CELL, 0xFFFF);
        game_play_fill_rect(px + CELL - 1, py, 1, CELL, 0x4208);
        game_play_fill_rect(px, py + CELL - 1, CELL, 1, 0x4208);
        return;
    }
    game_play_fill_rect(px, py, CELL, CELL, COL_OPEN);
    if (state[y][x] == ST_MINE) {
        game_play_fill_circle(px + CELL / 2, py + CELL / 2, CELL / 3, COL_MINE);
        return;
    }
    if (state[y][x] == ST_FLAG) {
        game_play_fill_circle(px + CELL / 2, py + CELL / 2, CELL / 4, COL_FLAG);
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

static void mines_redraw() {
    game_play_clear(COL_BG);
    for (int y = 0; y < GH; y++)
        for (int x = 0; x < GW; x++)
            draw_cell(x, y);
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

static void mines_reset_board() {
    memset(state, 0, sizeof(state));
    memset(board, 0, sizeof(board));
    first_tap = true;
    revealed = 0;
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
    mines_reset_board();
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
}

void game_mines_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;

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
                const int x = (in.play_x - OFF_X) / CELL;
                const int y = (in.play_y - OFF_Y) / CELL;
                if (x < 0 || x >= GW || y < 0 || y >= GH) continue;

                if (first_tap) {
                    first_tap = false;
                    place_mines(x, y);
                    start_ms = millis();
                }

                if (state[y][x] != ST_COVER) continue;

                if (board[y][x] == -1) {
                    state[y][x] = ST_MINE;
                    draw_cell(x, y);
                    buzzer_play(SFX_BOMB);
                    lives--;
                    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
                    if (lives <= 0) {
                        dead = true;
                        break;
                    }
                    mines_reset_board();
                    continue;
                }
                flood(x, y);
                buzzer_play(SFX_TICK);
                score = revealed * 10;
                if (revealed >= GW * GH - MINES) {
                    score += (int)((millis() - start_ms) / 100);
                    won = true;
                }
                game_hud_set_score(hud, score);
            }
            game_frame_delay();
        }

        if (game_hud_end_game(hud, score, won) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
