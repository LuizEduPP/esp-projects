#include "game_runtime.h"
#include "engine.h"
#include <Arduino.h>

void game_run(const GameEntry* entry) {
    if (!entry) return;

    Serial.printf("[GAME] start %s (%s)\n", entry->title, entry->engine);

    const GameRunFn run = engine_runner(entry->engine);
    if (run) run(entry);
    else Serial.printf("[GAME] unknown engine: %s\n", entry->engine);
}
