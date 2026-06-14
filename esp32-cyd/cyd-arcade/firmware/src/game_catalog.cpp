#include "game_catalog.h"
#include <string.h>

static const GameEntry k_builtin[] = {
    {"Snake",       "snake",    0x00FF00, 150},
    {"Arkanoid",    "breakout", 0xFF6600, 100},
    {"Tetris",      "tetris",   0x00F0F0, 600},
    {"Pong",        "pong",     0xFFFFFF, 80},
    {"Dodge",       "dodge",    0xFFE000, 80},
    {"Simon",       "simon",    0xFF0000, 500},
    {"Minesweeper", "mines",    0xC0C0C0, 0},
    {"Velha",       "velha",    0x00D4FF, 0},
};

int game_catalog_count() {
    return (int)(sizeof(k_builtin) / sizeof(k_builtin[0]));
}

void game_catalog_list(GameEntry* out, int max_n) {
    if (!out || max_n <= 0) return;
    int n = game_catalog_count();
    if (n > max_n) n = max_n;
    memcpy(out, k_builtin, (size_t)n * sizeof(GameEntry));
}
