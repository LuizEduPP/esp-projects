#include "engine.h"
#include "games_fwd.h"
#include <string.h>

typedef struct {
    const char* id;
    const char* score_key;
    EngineId engine;
    GameRunFn run;
} EngineEntry;

static const EngineEntry k_engines[] = {
    {"snake",    "hi_snake",  ENGINE_SNAKE,    game_snake_run},
    {"breakout", "hi_break",  ENGINE_BREAKOUT, game_breakout_run},
    {"tetris",   "hi_tetris", ENGINE_TETRIS,   game_tetris_run},
    {"pong",     "hi_pong",   ENGINE_PONG,     game_pong_run},
    {"dodge",    "hi_dodge",  ENGINE_DODGE,    game_dodge_run},
    {"simon",    "hi_simon",  ENGINE_SIMON,    game_simon_run},
    {"mines",    "hi_mines",  ENGINE_MINES,    game_mines_run},
    {"velha",    "hi_velha",  ENGINE_VELHA,    game_velha_run},
};

EngineId engine_id(const char* engine) {
    if (!engine) return ENGINE_UNKNOWN;
    if (strcmp(engine, "arkanoid") == 0) return ENGINE_BREAKOUT;
    for (size_t i = 0; i < sizeof(k_engines) / sizeof(k_engines[0]); i++) {
        if (strcmp(engine, k_engines[i].id) == 0)
            return k_engines[i].engine;
    }
    return ENGINE_UNKNOWN;
}

const char* engine_score_key(const char* engine) {
    const EngineId id = engine_id(engine);
    for (size_t i = 0; i < sizeof(k_engines) / sizeof(k_engines[0]); i++) {
        if (k_engines[i].engine == id)
            return k_engines[i].score_key;
    }
    return nullptr;
}

GameRunFn engine_runner(const char* engine) {
    const EngineId id = engine_id(engine);
    for (size_t i = 0; i < sizeof(k_engines) / sizeof(k_engines[0]); i++) {
        if (k_engines[i].engine == id)
            return k_engines[i].run;
    }
    return nullptr;
}
