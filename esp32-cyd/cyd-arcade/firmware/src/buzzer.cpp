#include "buzzer.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>
#include <Preferences.h>

#define SOUND_PREF_KEY "sound_on"

static bool s_ready = false;
static bool s_sound_on = true;

typedef struct {
    uint16_t freq;
    uint16_t ms;
    uint8_t gap;
} BuzzerNote;

static const uint16_t SIMON_FREQ[4] = {659, 784, 988, 1175};

static void buzzer_save_sound();

static void buzzer_load_sound() {
    Preferences prefs;
    prefs.begin("cyd-arcade", true);
    bool migrate = false;
    if (prefs.isKey(SOUND_PREF_KEY)) {
        s_sound_on = prefs.getBool(SOUND_PREF_KEY, true);
    } else if (prefs.isKey("vol_pct")) {
        s_sound_on = prefs.getUChar("vol_pct", 70) > 0;
        migrate = true;
    } else if (prefs.isKey("volume")) {
        s_sound_on = prefs.getUChar("volume", 70) > 0;
        migrate = true;
    } else {
        s_sound_on = true;
    }
    prefs.end();
    if (migrate)
        buzzer_save_sound();
}

static void buzzer_save_sound() {
    Preferences prefs;
    prefs.begin("cyd-arcade", false);
    prefs.putBool(SOUND_PREF_KEY, s_sound_on);
    prefs.remove("vol_pct");
    prefs.remove("volume");
    prefs.end();
}

static uint16_t passive_hz(uint16_t freq) {
    if (freq < 350) return (uint16_t)(freq * 4);
    if (freq < 700) return (uint16_t)(freq * 2);
    return freq;
}

static void play_notes(const BuzzerNote* notes, int count) {
    for (int i = 0; i < count; i++) {
        if (notes[i].freq)
            buzzer_tone(notes[i].freq, notes[i].ms);
        else if (notes[i].ms)
            delay(notes[i].ms);
        if (notes[i].gap)
            delay(notes[i].gap);
    }
}

void buzzer_init() {
    buzzer_load_sound();
    ledcSetup(BUZZER_LEDC_CH, 2700, BUZZER_LEDC_BITS);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
    ledcWrite(BUZZER_LEDC_CH, 0);
    s_ready = true;
}

bool buzzer_sound_on() {
    return s_sound_on;
}

void buzzer_sound_toggle() {
    s_sound_on = !s_sound_on;
    buzzer_save_sound();
}

void buzzer_stop() {
    if (!s_ready) return;
    ledcWrite(BUZZER_LEDC_CH, 0);
    display_brightness_refresh();
}

void buzzer_tone(uint16_t freq_hz, uint16_t duration_ms) {
    if (!s_sound_on || freq_hz == 0 || duration_ms == 0) return;
    freq_hz = passive_hz(freq_hz);
    ledcAttachPin(BUZZER_PIN, BUZZER_LEDC_CH);
    s_ready = true;
    if (ledcWriteTone(BUZZER_LEDC_CH, freq_hz) == 0) {
        ledcSetup(BUZZER_LEDC_CH, freq_hz, BUZZER_LEDC_BITS);
        ledcWrite(BUZZER_LEDC_CH, 512);
    }
    delay(duration_ms);
    buzzer_stop();
}

void buzzer_simon_tone(int index) {
    if (index < 0 || index > 3) return;
    buzzer_tone(SIMON_FREQ[index], 180);
}

void buzzer_play(BuzzerSfx sfx) {
    static const BuzzerNote tick[]      = {{988, 16, 0}};
    static const BuzzerNote select[]    = {{1175, 40, 6}, {988, 30, 0}};
    static const BuzzerNote score[]     = {{784, 35, 8}, {988, 40, 8}, {1175, 50, 8}, {1318, 65, 0}};
    static const BuzzerNote hit[]       = {{1046, 30, 4}, {880, 22, 4}, {1175, 28, 0}};
    static const BuzzerNote drop[]      = {{587, 22, 4}, {494, 28, 4}, {440, 32, 0}};
    static const BuzzerNote level[]     = {{659, 50, 12}, {784, 50, 12}, {988, 50, 12}, {1175, 50, 12}, {1318, 90, 0}};
    static const BuzzerNote line[]      = {{659, 38, 8}, {784, 38, 8}, {988, 42, 8}, {1175, 42, 8}, {1318, 42, 8}, {1568, 110, 0}};
    static const BuzzerNote error[]     = {{622, 85, 12}, {523, 85, 12}, {440, 85, 12}, {370, 110, 0}};
    static const BuzzerNote win[]       = {{784, 55, 10}, {988, 55, 10}, {1175, 55, 10}, {1318, 55, 10}, {1568, 55, 10}, {1760, 160, 0}};
    static const BuzzerNote lose[]      = {{622, 95, 14}, {523, 95, 14}, {440, 95, 14}, {349, 130, 0}};
    static const BuzzerNote record[]    = {{1175, 50, 8}, {1318, 50, 8}, {1568, 50, 8}, {1760, 50, 8}, {1976, 150, 0}};
    static const BuzzerNote startup[]   = {{659, 45, 8}, {784, 45, 8}, {988, 50, 8}, {1175, 55, 8}, {1318, 70, 0}};
    static const BuzzerNote bomb[]      = {{180, 45, 0}, {130, 55, 6}, {90, 65, 6}, {60, 90, 0}};
    static const BuzzerNote flip[]      = {{740, 22, 4}, {880, 28, 0}};
    static const BuzzerNote match[]     = {{988, 38, 6}, {1175, 42, 6}, {1318, 48, 6}, {1568, 70, 0}};
    static const BuzzerNote miss[]      = {{523, 65, 8}, {440, 75, 8}, {370, 85, 0}};
    static const BuzzerNote shoot[]     = {{1175, 28, 4}, {1318, 32, 4}, {1568, 40, 0}};

    switch (sfx) {
    case SFX_TICK:    play_notes(tick, 1); break;
    case SFX_SELECT:  play_notes(select, 2); break;
    case SFX_SCORE:   play_notes(score, 4); break;
    case SFX_HIT:     play_notes(hit, 3); break;
    case SFX_DROP:    play_notes(drop, 3); break;
    case SFX_LEVEL:   play_notes(level, 5); break;
    case SFX_LINE:    play_notes(line, 6); break;
    case SFX_ERROR:   play_notes(error, 4); break;
    case SFX_WIN:     play_notes(win, 6); break;
    case SFX_LOSE:    play_notes(lose, 4); break;
    case SFX_RECORD:  play_notes(record, 5); break;
    case SFX_STARTUP: play_notes(startup, 5); break;
    case SFX_BOMB:    play_notes(bomb, 4); break;
    case SFX_FLIP:    play_notes(flip, 2); break;
    case SFX_MATCH:   play_notes(match, 4); break;
    case SFX_MISS:    play_notes(miss, 3); break;
    case SFX_SHOOT:   play_notes(shoot, 3); break;
    }
}
