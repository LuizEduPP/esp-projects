#include "engine.h"
#include <string.h>

EngineId engine_id(const char* engine) {
    if (!engine) return ENGINE_UNKNOWN;
    if (strcmp(engine, "snake") == 0) return ENGINE_SNAKE;
    if (strcmp(engine, "breakout") == 0 || strcmp(engine, "arkanoid") == 0) return ENGINE_BREAKOUT;
    if (strcmp(engine, "tetris") == 0) return ENGINE_TETRIS;
    if (strcmp(engine, "pong") == 0) return ENGINE_PONG;
    if (strcmp(engine, "dodge") == 0) return ENGINE_DODGE;
    if (strcmp(engine, "simon") == 0) return ENGINE_SIMON;
    if (strcmp(engine, "mines") == 0) return ENGINE_MINES;
    if (strcmp(engine, "velha") == 0) return ENGINE_VELHA;
    if (strcmp(engine, "reaction") == 0) return ENGINE_REACTION;
    if (strcmp(engine, "rps") == 0) return ENGINE_RPS;
    if (strcmp(engine, "jump") == 0) return ENGINE_JUMP;
    if (strcmp(engine, "bounce") == 0) return ENGINE_BOUNCE;
    if (strcmp(engine, "stack") == 0) return ENGINE_STACK;
    return ENGINE_UNKNOWN;
}

const char* engine_score_key(const char* engine) {
    switch (engine_id(engine)) {
    case ENGINE_SNAKE: return "hi_snake";
    case ENGINE_BREAKOUT: return "hi_break";
    case ENGINE_TETRIS: return "hi_tetris";
    case ENGINE_PONG: return "hi_pong";
    case ENGINE_DODGE: return "hi_dodge";
    case ENGINE_SIMON: return "hi_simon";
    case ENGINE_MINES: return "hi_mines";
    case ENGINE_VELHA: return "hi_velha";
    case ENGINE_REACTION: return "hi_reaction";
    case ENGINE_RPS: return "hi_rps";
    case ENGINE_JUMP: return "hi_jump";
    case ENGINE_BOUNCE: return "hi_bounce";
    case ENGINE_STACK: return "hi_stack";
    default: return nullptr;
    }
}

extern void game_snake_run(const GameEntry* cfg);
extern void game_breakout_run(const GameEntry* cfg);
extern void game_tetris_run(const GameEntry* cfg);
extern void game_pong_run(const GameEntry* cfg);
extern void game_dodge_run(const GameEntry* cfg);
extern void game_simon_run(const GameEntry* cfg);
extern void game_mines_run(const GameEntry* cfg);
extern void game_velha_run(const GameEntry* cfg);
extern void game_reaction_run(const GameEntry* cfg);
extern void game_rps_run(const GameEntry* cfg);
extern void game_jump_run(const GameEntry* cfg);
extern void game_bounce_run(const GameEntry* cfg);
extern void game_stack_run(const GameEntry* cfg);

GameRunFn engine_runner(const char* engine) {
    switch (engine_id(engine)) {
    case ENGINE_SNAKE: return game_snake_run;
    case ENGINE_BREAKOUT: return game_breakout_run;
    case ENGINE_TETRIS: return game_tetris_run;
    case ENGINE_PONG: return game_pong_run;
    case ENGINE_DODGE: return game_dodge_run;
    case ENGINE_SIMON: return game_simon_run;
    case ENGINE_MINES: return game_mines_run;
    case ENGINE_VELHA: return game_velha_run;
    case ENGINE_REACTION: return game_reaction_run;
    case ENGINE_RPS: return game_rps_run;
    case ENGINE_JUMP: return game_jump_run;
    case ENGINE_BOUNCE: return game_bounce_run;
    case ENGINE_STACK: return game_stack_run;
    default: return nullptr;
    }
}
