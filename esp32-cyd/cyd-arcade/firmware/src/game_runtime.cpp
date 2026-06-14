#include "game_runtime.h"
#include "engine.h"
#include "buzzer.h"
#include <Arduino.h>

void game_run(const GameEntry* entry) {
    if (!entry) return;

    buzzer_init();

    const GameRunFn run = engine_runner(entry->engine);
    if (run) run(entry);
}
