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

static const uint16_t SIMON_FREQ[4] = {784, 988, 1175, 1568};

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
    buzzer_tone(SIMON_FREQ[index], 200);
}

void buzzer_play(BuzzerSfx sfx) {
    static const BuzzerNote tick[]      = {{1200, 30, 0}};
    static const BuzzerNote select[]    = {{988, 50, 0}};
    static const BuzzerNote score[]     = {{784, 40, 15}, {1175, 60, 0}};
    static const BuzzerNote hit[]       = {{880, 28, 0}};
    static const BuzzerNote drop[]      = {{620, 22, 0}};
    static const BuzzerNote level[]     = {{784, 60, 20}, {988, 60, 20}, {1175, 80, 0}};
    static const BuzzerNote line[]      = {{880, 50, 12}, {988, 50, 12}, {1175, 60, 12}, {1568, 100, 0}};
    static const BuzzerNote error[]     = {{740, 100, 25}, {520, 140, 0}};
    static const BuzzerNote win[]       = {{784, 75, 18}, {988, 75, 18}, {1175, 75, 18}, {1568, 170, 0}};
    static const BuzzerNote lose[]      = {{740, 120, 20}, {620, 120, 20}, {520, 200, 0}};
    static const BuzzerNote record[]    = {{1175, 65, 15}, {1568, 65, 15}, {1760, 150, 0}};
    static const BuzzerNote startup[]   = {{880, 55, 12}, {988, 55, 12}, {1175, 70, 12}, {1568, 100, 0}};
    static const BuzzerNote bomb[]      = {{220, 55, 0}, {160, 65, 8}, {110, 75, 8}, {70, 110, 0}};

    switch (sfx) {
    case SFX_TICK:    play_notes(tick, 1); break;
    case SFX_SELECT:  play_notes(select, 1); break;
    case SFX_SCORE:   play_notes(score, 2); break;
    case SFX_HIT:     play_notes(hit, 1); break;
    case SFX_DROP:    play_notes(drop, 1); break;
    case SFX_LEVEL:   play_notes(level, 3); break;
    case SFX_LINE:    play_notes(line, 4); break;
    case SFX_ERROR:   play_notes(error, 2); break;
    case SFX_WIN:     play_notes(win, 4); break;
    case SFX_LOSE:    play_notes(lose, 3); break;
    case SFX_RECORD:  play_notes(record, 3); break;
    case SFX_STARTUP: play_notes(startup, 4); break;
    case SFX_BOMB:    play_notes(bomb, 4); break;
    }
}
