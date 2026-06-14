#include "score_store.h"
#include "engine.h"
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>
#include <string.h>

static const char* name_key_for(const char* engine) {
    switch (engine_id(engine)) {
    case ENGINE_SNAKE: return "nm_snake";
    case ENGINE_BREAKOUT: return "nm_break";
    case ENGINE_TETRIS: return "nm_tetris";
    case ENGINE_PONG: return "nm_pong";
    case ENGINE_DODGE: return "nm_dodge";
    case ENGINE_SIMON: return "nm_simon";
    case ENGINE_MINES: return "nm_mines";
    case ENGINE_VELHA: return "nm_velha";
    default: return nullptr;
    }
}

int score_store_get(const char* engine) {
    const char* key = engine_score_key(engine);
    if (!key) return 0;
    Preferences prefs;
    prefs.begin("cyd-arcade", true);
    const int v = prefs.getInt(key, 0);
    prefs.end();
    return v;
}

bool score_store_save(const char* engine, int score) {
    const char* key = engine_score_key(engine);
    if (!key || score <= 0) return false;
    Preferences prefs;
    prefs.begin("cyd-arcade", false);
    const int prev = prefs.getInt(key, 0);
    if (score > prev) {
        prefs.putInt(key, score);
        prefs.end();
        Serial.printf("[SCORE] %s novo recorde %d\n", engine, score);
        return true;
    }
    prefs.end();
    return false;
}

void score_store_get_name(const char* engine, char* out, size_t out_len) {
    if (!out || out_len == 0) return;
    out[0] = '\0';
    const char* key = name_key_for(engine);
    if (!key) return;
    Preferences prefs;
    prefs.begin("cyd-arcade", true);
    prefs.getString(key, out, out_len);
    prefs.end();
}

void score_store_set_name(const char* engine, const char* name) {
    const char* key = name_key_for(engine);
    if (!key || !name) return;
    Preferences prefs;
    prefs.begin("cyd-arcade", false);
    prefs.putString(key, name);
    prefs.end();
    Serial.printf("[SCORE] %s nome=%s\n", engine, name);
}
