#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "score_store.h"
#include "ui_theme.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>
#include <string.h>

#define GRID 3
#define MODE_H 32

#define COL_BG   ui_rgb565(0x0B0F1A)
#define COL_GRID ui_rgb565(0x3D4F66)
#define COL_X    ui_rgb565(0x00D4FF)
#define COL_O    ui_rgb565(0xF87171)

static int8_t board[GRID][GRID];
static int wins;
static int CELL, OFF_X, OFF_Y;
static bool two_player;
static int8_t turn;
static bool waiting;
static uint32_t wait_until;

static int check_winner() {
    for (int i = 0; i < GRID; i++) {
        if (board[i][0] && board[i][0] == board[i][1] && board[i][1] == board[i][2])
            return board[i][0];
        if (board[0][i] && board[0][i] == board[1][i] && board[1][i] == board[2][i])
            return board[0][i];
    }
    if (board[0][0] && board[0][0] == board[1][1] && board[1][1] == board[2][2])
        return board[0][0];
    if (board[0][2] && board[0][2] == board[1][1] && board[1][1] == board[2][0])
        return board[0][2];
    return 0;
}

static bool board_full() {
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            if (!board[r][c]) return false;
    return true;
}

static int cell_index(int r, int c) { return r * GRID + c; }

static bool find_move_for(int player, int* out_r, int* out_c) {
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (board[r][c]) continue;
            board[r][c] = (int8_t)player;
            const int w = check_winner();
            board[r][c] = 0;
            if (w == player) {
                *out_r = r;
                *out_c = c;
                return true;
            }
        }
    }
    return false;
}

static void random_move(int* out_r, int* out_c) {
    int empty[9];
    int n = 0;
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            if (!board[r][c]) empty[n++] = cell_index(r, c);
    if (n == 0) return;
    const int pick = empty[random(0, n)];
    *out_r = pick / GRID;
    *out_c = pick % GRID;
}

static void cpu_move_weak() {
    int r = -1, c = -1;
    const int roll = random(0, 100);

    if (roll < 50) {
        random_move(&r, &c);
    } else if (roll < 75) {
        if (!find_move_for(1, &r, &c))
            random_move(&r, &c);
    } else {
        if (!find_move_for(2, &r, &c))
            random_move(&r, &c);
    }

    if (r >= 0) board[r][c] = 2;
}

static void draw_x(int cx, int cy, int m) {
    const int x0 = PLAY_X + cx - m;
    const int y0 = PLAY_Y + cy - m;
    const int x1 = PLAY_X + cx + m;
    const int y1 = PLAY_Y + cy + m;
    tft.drawLine(x0, y0, x1, y1, COL_X);
    tft.drawLine(x1, y0, x0, y1, COL_X);
    tft.drawLine(x0 + 1, y0, x1, y1, COL_X);
    tft.drawLine(x1 - 1, y0, x0, y1, COL_X);
    tft.drawLine(x0, y0 + 1, x1, y1, COL_X);
    tft.drawLine(x1, y0 + 1, x0, y1, COL_X);
}

static void draw_mark(int r, int c) {
    const int cx = OFF_X + c * CELL + CELL / 2;
    const int cy = OFF_Y + r * CELL + CELL / 2;
    const int m = CELL / 3;
    if (board[r][c] == 1) {
        draw_x(cx, cy, m);
    } else if (board[r][c] == 2) {
        game_play_fill_circle(cx, cy, m, COL_O);
        game_play_fill_circle(cx, cy, m - 4, COL_BG);
    }
}

static void draw_mode_bar() {
    const int y = OFF_Y + CELL * GRID + 6;
    const int bw = 72;
    const int gap = 12;
    const int x1 = (PLAY_W - bw * 2 - gap) / 2;
    const int x2 = x1 + bw + gap;
    const uint16_t on = ui_theme_get()->accent;
    const uint16_t off = ui_theme_get()->card;

    game_play_fill_rect(0, y - 2, PLAY_W, MODE_H, COL_BG);
    game_play_fill_round_rect(x1, y, bw, 24, 6, two_player ? off : on);
    game_play_fill_round_rect(x2, y, bw, 24, 6, two_player ? on : off);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(two_player ? ui_theme_get()->text_mute : ui_theme_get()->text_hi,
                     two_player ? off : on);
    tft.drawString("1P CPU", PLAY_X + x1 + bw / 2, PLAY_Y + y + 12, 1);
    tft.setTextColor(two_player ? ui_theme_get()->text_hi : ui_theme_get()->text_mute,
                     two_player ? on : off);
    tft.drawString("2P", PLAY_X + x2 + bw / 2, PLAY_Y + y + 12, 1);
}

static void draw_grid() {
    game_play_clear(COL_BG);
    for (int i = 1; i < GRID; i++) {
        game_play_fill_rect(OFF_X + i * CELL - 2, OFF_Y, 4, CELL * GRID, COL_GRID);
        game_play_fill_rect(OFF_X, OFF_Y + i * CELL - 2, CELL * GRID, 4, COL_GRID);
    }
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            if (board[r][c]) draw_mark(r, c);
    draw_mode_bar();
}

static int hit_cell(int16_t tx, int16_t ty) {
    if (tx < OFF_X || ty < OFF_Y) return -1;
    const int c = (tx - OFF_X) / CELL;
    const int r = (ty - OFF_Y) / CELL;
    if (r < 0 || r >= GRID || c < 0 || c >= GRID) return -1;
    return cell_index(r, c);
}

static bool hit_mode_toggle(int16_t tx, int16_t ty) {
    const int y = OFF_Y + CELL * GRID + 6;
    if (ty < y || ty >= y + 24) return false;
    const int bw = 72;
    const int gap = 12;
    const int x1 = (PLAY_W - bw * 2 - gap) / 2;
    const int x2 = x1 + bw + gap;
    if (tx >= x1 && tx < x1 + bw) {
        two_player = false;
        return true;
    }
    if (tx >= x2 && tx < x2 + bw) {
        two_player = true;
        return true;
    }
    return false;
}

static void velha_reset_board() {
    memset(board, 0, sizeof(board));
    turn = 1;
    const int grid_sz = min(PLAY_W - 24, PLAY_H - MODE_H - 20);
    CELL = grid_sz / GRID;
    OFF_X = (PLAY_W - CELL * GRID) / 2;
    OFF_Y = (PLAY_H - CELL * GRID - MODE_H - 8) / 2;
    if (OFF_Y < 4) OFF_Y = 4;
    draw_grid();
}

static void end_round(GameHud* hud, const char* title, uint16_t col, bool player_won) {
    if (player_won && !two_player) {
        wins++;
        game_hud_set_score(hud, wins);
    }
    game_play_toast(title, "Toque p/ continuar", col, COL_BG);
    waiting = true;
    wait_until = millis() + 1200;
}

void game_velha_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin("Velha", "velha", 0x00D4FF);
    if (!hud) return;

    wins = 0;
    two_player = false;
    waiting = false;
    velha_reset_board();
    game_hud_set_score(hud, 0);

    GameInput in;
    for (;;) {
        game_frame_tick();
        game_input_poll(&in);
        if (game_hud_poll(hud)) {
            if (wins > 0) score_store_save(hud->engine, wins);
            game_hud_end(hud);
            return;
        }
        if (game_hud_consume_resume_redraw(hud))
            draw_grid();

        if (waiting) {
            if (millis() < wait_until) {
                game_frame_delay();
                continue;
            }
            waiting = false;
            velha_reset_board();
        }

        if (in.just_pressed && in.y >= PLAY_Y) {
            if (hit_mode_toggle(in.play_x, in.play_y)) {
                velha_reset_board();
                continue;
            }

            const int cell = hit_cell(in.play_x, in.play_y);
            if (cell < 0 || check_winner()) continue;
            const int r = cell / GRID;
            const int c = cell % GRID;
            if (board[r][c]) continue;

            const int8_t mark = two_player ? turn : 1;
            board[r][c] = mark;
            draw_mark(r, c);

            int w = check_winner();
            if (w) {
                if (two_player)
                    end_round(hud, w == 1 ? "X venceu!" : "O venceu!", w == 1 ? COL_X : COL_O, false);
                else if (w == 1)
                    end_round(hud, "Vitoria!", COL_X, true);
                else
                    end_round(hud, "CPU venceu", COL_O, false);
                continue;
            }
            if (board_full()) {
                end_round(hud, "Empate", COL_GRID, false);
                continue;
            }

            if (two_player) {
                turn = (turn == 1) ? 2 : 1;
                continue;
            }

            cpu_move_weak();
            draw_grid();

            w = check_winner();
            if (w == 2)
                end_round(hud, "CPU venceu", COL_O, false);
            else if (board_full())
                end_round(hud, "Empate", COL_GRID, false);
        }
        game_frame_delay();
    }
}
