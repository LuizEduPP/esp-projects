#include "game_runtime.h"
#include "engine.h"

void game_run(const GameEntry* entry) {
    if (!entry) return;
    const GameRunFn run = engine_runner(entry->engine);
    if (run) run(entry);
}
