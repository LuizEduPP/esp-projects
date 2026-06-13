#pragma once
#include "game_catalog.h"

#define LAUNCHER_SETTINGS (-2)

void launcher_draw_header(int count);
int launcher_show(GameEntry* games, int count);
