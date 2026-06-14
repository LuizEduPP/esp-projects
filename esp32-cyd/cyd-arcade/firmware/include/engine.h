#pragma once
#include "game_catalog.h"

typedef enum {
    ENGINE_SNAKE = 0,
    ENGINE_BREAKOUT,
    ENGINE_TETRIS,
    ENGINE_PONG,
    ENGINE_DODGE,
    ENGINE_SIMON,
    ENGINE_MINES,
    ENGINE_VELHA,
    ENGINE_UNKNOWN,
} EngineId;

EngineId engine_id(const char* engine);
const char* engine_score_key(const char* engine);
typedef void (*GameRunFn)(const GameEntry*);
GameRunFn engine_runner(const char* engine);
