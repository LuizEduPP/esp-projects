#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "buzzer.h"
#include "ui_theme.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>
#include <string.h>

#define GRID 3
#define MODE_H 32
#define ROUND_MS 900

#define COL_BG   ui_rgb565(0x0B0F1A)
#define COL_GRID ui_rgb565(0x3D4F66)
#define COL_X    ui_rgb565(0x00D4FF)
#define COL_O    ui_rgb565(0xF87171)

static int8_t board[GRID][GRID];
static int wins;
static int cpu_wins;
static int lives;
static int CELL, OFF_X, OFF_Y;
static bool two_player;
static int8_t turn;
static bool round_wait;
static uint32_t wait_until;
static bool round_won;
static bool round_lost;
static bool round_draw;

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

static int count_winning_moves(int player) {
    int n = 0;
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (board[r][c]) continue;
            board[r][c] = (int8_t)player;
            if (check_winner() == player) n++;
            board[r][c] = 0;
        }
    }
    return n;
}

static bool find_fork_for(int player, int* out_r, int* out_c) {
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (board[r][c]) continue;
            board[r][c] = (int8_t)player;
            const bool fork = count_winning_moves(player) >= 2;
            board[r][c] = 0;
            if (fork) {
                *out_r = r;
                *out_c = c;
                return true;
            }
        }
    }
    return false;
}

static bool block_opponent_fork(int* out_r, int* out_c) {
    int fr = -1, fc = -1;
    if (!find_fork_for(1, &fr, &fc))
        return false;

    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (board[r][c]) continue;
            board[r][c] = 2;
            const bool still_fork = find_fork_for(1, &fr, &fc);
            board[r][c] = 0;
            if (!still_fork) {
                *out_r = r;
                *out_c = c;
                return true;
            }
        }
    }

    if (!board[fr][fc]) {
        *out_r = fr;
        *out_c = fc;
        return true;
    }
    return false;
}

static bool opposite_corner_move(int* out_r, int* out_c) {
    static const int pairs[4][4] = {
        {0, 0, 2, 2}, {2, 2, 0, 0}, {0, 2, 2, 0}, {2, 0, 0, 2},
    };
    for (int i = 0; i < 4; i++) {
        const int o_r = pairs[i][0], o_c = pairs[i][1];
        const int p_r = pairs[i][2], p_c = pairs[i][3];
        if (board[o_r][o_c] == 1 && !board[p_r][p_c]) {
            *out_r = p_r;
            *out_c = p_c;
            return true;
        }
    }
    return false;
}

static bool pick_corner(int* out_r, int* out_c) {
    static const int cr[4][2] = {{0, 0}, {0, 2}, {2, 0}, {2, 2}};
    int avail[4];
    int n = 0;
    for (int i = 0; i < 4; i++) {
        if (!board[cr[i][0]][cr[i][1]])
            avail[n++] = i;
    }
    if (n == 0) return false;
    const int pick = avail[random(0, n)];
    *out_r = cr[pick][0];
    *out_c = cr[pick][1];
    return true;
}

static bool pick_any_empty(int* out_r, int* out_c) {
    int empty[9];
    int n = 0;
    for (int r = 0; r < GRID; r++)
        for (int c = 0; c < GRID; c++)
            if (!board[r][c]) empty[n++] = cell_index(r, c);
    if (n == 0) return false;
    const int pick = empty[random(0, n)];
    *out_r = pick / GRID;
    *out_c = pick % GRID;
    return true;
}

static bool place_cpu(int r, int c) {
    if (r < 0 || c < 0 || board[r][c]) return false;
    board[r][c] = 2;
    return true;
}

static int minimax_score(int player, int depth, int alpha, int beta);

static void cpu_move_strong() {
    int best_score = -1000;
    int br = -1, bc = -1;
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (board[r][c]) continue;
            board[r][c] = 2;
            const int s = minimax_score(1, 0, -1000, 1000);
            board[r][c] = 0;
            if (s > best_score) {
                best_score = s;
                br = r;
                bc = c;
            }
        }
    }
    place_cpu(br, bc);
}

/* Heuristica classica — forte, mas nao invencivel como minimax */
static void cpu_move_heuristic() {
    int r = -1, c = -1;

    if (find_move_for(2, &r, &c)) { place_cpu(r, c); return; }
    if (find_move_for(1, &r, &c)) { place_cpu(r, c); return; }
    if (find_fork_for(2, &r, &c)) { place_cpu(r, c); return; }
    if (block_opponent_fork(&r, &c)) { place_cpu(r, c); return; }
    if (!board[1][1]) { place_cpu(1, 1); return; }
    if (opposite_corner_move(&r, &c)) { place_cpu(r, c); return; }
    if (pick_corner(&r, &c)) { place_cpu(r, c); return; }
    if (pick_any_empty(&r, &c)) place_cpu(r, c);
}

/*
 * Fase 0 — novato: quase aleatorio, raramente bloqueia.
 * Fase 1 — aprendiz: bloqueia sempre, ataca as vezes, prefere centro.
 * Fase 2 — tatico: heuristica completa (fork, cantos, etc.).
 * Fase 3+ — mestre: minimax perfeito.
 */
static void cpu_move_for_phase(int phase) {
    int r = -1, c = -1;

    if (phase >= 3) {
        cpu_move_strong();
        return;
    }

    if (phase >= 2) {
        cpu_move_heuristic();
        return;
    }

    if (phase >= 1) {
        if (find_move_for(2, &r, &c)) { place_cpu(r, c); return; }
        if (find_move_for(1, &r, &c)) { place_cpu(r, c); return; }
        if (!board[1][1] && random(0, 100) < 70) { place_cpu(1, 1); return; }
        if (random(0, 100) < 55 && pick_corner(&r, &c)) { place_cpu(r, c); return; }
        if (pick_any_empty(&r, &c)) place_cpu(r, c);
        return;
    }

    /* phase 0 */
    if (random(0, 100) < 30 && find_move_for(1, &r, &c)) { place_cpu(r, c); return; }
    if (random(0, 100) < 20 && find_move_for(2, &r, &c)) { place_cpu(r, c); return; }
    if (random(0, 100) < 25 && !board[1][1]) { place_cpu(1, 1); return; }
    if (pick_any_empty(&r, &c)) place_cpu(r, c);
}

static int minimax_score(int player, int depth, int alpha, int beta) {
    const int w = check_winner();
    if (w == 2) return 10 - depth;
    if (w == 1) return depth - 10;
    if (board_full()) return 0;

    if (player == 2) {
        int best = -1000;
        for (int r = 0; r < GRID; r++) {
            for (int c = 0; c < GRID; c++) {
                if (board[r][c]) continue;
                board[r][c] = 2;
                best = max(best, minimax_score(1, depth + 1, alpha, beta));
                board[r][c] = 0;
                alpha = max(alpha, best);
                if (beta <= alpha) break;
            }
            if (beta <= alpha) break;
        }
        return best;
    }

    int best = 1000;
    for (int r = 0; r < GRID; r++) {
        for (int c = 0; c < GRID; c++) {
            if (board[r][c]) continue;
            board[r][c] = 1;
            best = min(best, minimax_score(2, depth + 1, alpha, beta));
            board[r][c] = 0;
            beta = min(beta, best);
            if (beta <= alpha) break;
        }
        if (beta <= alpha) break;
    }
    return best;
}

static void show_cpu_phase_toast(GameHud* hud, int phase) {
    if (!hud) return;
    static const char* labels[] = {"CPU facil", "CPU medio", "CPU forte", "CPU mestre"};
    const int idx = phase < 0 ? 0 : (phase > 3 ? 3 : phase);
    game_hud_show_toast(hud, labels[idx]);
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

static void layout_grid() {
    const int grid_sz = min(PLAY_W - 24, PLAY_H - MODE_H - 20);
    CELL = grid_sz / GRID;
    OFF_X = (PLAY_W - CELL * GRID) / 2;
    OFF_Y = (PLAY_H - CELL * GRID - MODE_H - 8) / 2;
    if (OFF_Y < 4) OFF_Y = 4;
}

static void velha_reset_board() {
    memset(board, 0, sizeof(board));
    turn = 1;
    layout_grid();
    draw_grid();
}

static void begin_round_wait(bool won, bool lost, bool draw) {
    round_won = won;
    round_lost = lost;
    round_draw = draw;
    round_wait = true;
    wait_until = millis() + ROUND_MS;

    if (draw) {
        buzzer_play(SFX_SELECT);
    } else if (won && !two_player) {
        wins++;
        buzzer_play(SFX_WIN);
    } else if (won) {
        buzzer_play(SFX_WIN);
    } else if (lost) {
        if (!two_player) {
            lives--;
            cpu_wins++;
        }
        buzzer_play(SFX_ERROR);
    }
}

static void velha_init(GameHud* hud) {
    wins = 0;
    cpu_wins = 0;
    lives = GAME_LIVES_DEFAULT;
    two_player = false;
    round_wait = false;
    velha_reset_board();
    game_hud_set_tier_mode(hud, HUD_TIER_CPU, true);
    game_hud_set_score_tag(hud, "Vit");
    game_hud_set_tier(hud, 0);
    game_hud_set_score(hud, 0);
    game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
}

void game_velha_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin(cfg->engine);
    if (!hud) return;

    bool retry = false;
    for (;;) {
        if (retry) game_hud_reset_play(hud);
        retry = true;
        velha_init(hud);

        GameInput in;
        bool dead = false;

        while (!dead) {
            game_frame_tick();
            game_input_poll(&in);
            if (game_hud_poll(hud)) {
                game_hud_end(hud);
                return;
            }
            if (game_hud_consume_resume_redraw(hud))
                draw_grid();

            if (round_wait) {
                if (millis() >= wait_until) {
                    round_wait = false;
                    if (round_won && !two_player)
                        game_hud_set_score(hud, wins);
                    if (round_lost && !two_player) {
                        game_hud_set_lives(hud, lives, GAME_LIVES_DEFAULT);
                        if (cpu_wins != hud->tier) {
                            game_hud_set_tier(hud, cpu_wins);
                            show_cpu_phase_toast(hud, cpu_wins);
                            buzzer_play(SFX_LEVEL);
                            draw_grid();
                        }
                        if (lives <= 0) {
                            dead = true;
                            break;
                        }
                    }
                    velha_reset_board();
                }
                game_frame_delay();
                continue;
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
                buzzer_play(SFX_TICK);

                int w = check_winner();
                if (w) {
                    if (two_player)
                        begin_round_wait(true, false, false);
                    else if (w == 1)
                        begin_round_wait(true, false, false);
                    else
                        begin_round_wait(false, true, false);
                    continue;
                }
                if (board_full()) {
                    begin_round_wait(false, false, true);
                    continue;
                }

                if (two_player) {
                    turn = (turn == 1) ? 2 : 1;
                    continue;
                }

                cpu_move_for_phase(cpu_wins);
                draw_grid();

                w = check_winner();
                if (w == 2)
                    begin_round_wait(false, true, false);
                else if (board_full())
                    begin_round_wait(false, false, true);
            }
            game_frame_delay();
        }

        const bool session_won = wins >= cpu_wins;
        if (game_hud_end_game(hud, wins, session_won) == GAME_END_MENU) break;
    }
    game_hud_end(hud);
}
