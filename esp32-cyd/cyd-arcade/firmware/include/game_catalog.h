#pragma once
#include <stdint.h>

#define MAX_GAMES 13
#define GAME_TITLE_LEN 24
#define GAME_ENGINE_LEN 16

typedef struct {
    char title[GAME_TITLE_LEN];
    char engine[GAME_ENGINE_LEN];
    uint32_t color;
    uint16_t speed;
} GameEntry;

int game_catalog_count();
void game_catalog_list(GameEntry* out, int max_n);
