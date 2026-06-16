#include "audio.h"
#include "hw_config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <driver/dac.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

/* ESP32-2432S028: GPIO26 = DAC2 -> SC8002B -> alto-falante 1.5W.
 * Nao usar I2S: ele reconfigura GPIO25 (CLK do touch) e pode silenciar o DAC. */
#define SOUND_PREF_KEY   "sound_on"
#define NOTE_QUEUE_LEN   64
#define DAC_PEAK         255

typedef struct {
    uint16_t freq;
    uint16_t ms;
    uint16_t gap;
} SfxNote;

typedef struct {
    uint16_t freq;
    uint16_t ms;
    uint8_t gap;
} SfxPhrase;

static bool s_ready = false;
static bool s_on = true;
static QueueHandle_t s_note_q = nullptr;
static TaskHandle_t s_task = nullptr;

static const uint16_t SIMON_FREQ[4] = {659, 784, 988, 1175};

static void save_pref();

static void load_pref() {
    Preferences prefs;
    prefs.begin("cyd-arcade", true);
    s_on = prefs.getBool(SOUND_PREF_KEY, true);
    prefs.end();
    if (!s_on) {
        prefs.begin("cyd-arcade", false);
        prefs.putBool(SOUND_PREF_KEY, true);
        prefs.end();
        s_on = true;
    }
}

static void save_pref() {
    Preferences prefs;
    prefs.begin("cyd-arcade", false);
    prefs.putBool(SOUND_PREF_KEY, s_on);
    prefs.end();
}

static void dac_silence() {
    dacWrite(SPEAKER_PIN, 0);
}

static void dac_hw_init() {
    if (s_ready) return;
    dac_output_enable(DAC_CHANNEL_2);
    dac_silence();
    s_ready = true;
    Serial.printf("[AUDIO] DAC GPIO%d pronto som=%s\n", SPEAKER_PIN, s_on ? "ligado" : "mutado");
}

static void play_tone(uint16_t freq_hz, uint16_t duration_ms) {
    if (!s_on || !s_ready || freq_hz == 0 || duration_ms == 0) return;

    const uint32_t half_us = 500000UL / freq_hz;
    const uint32_t end_ms = millis() + duration_ms;

    while ((int32_t)(millis() - end_ms) < 0) {
        dacWrite(SPEAKER_PIN, DAC_PEAK);
        delayMicroseconds(half_us);
        dacWrite(SPEAKER_PIN, 0);
        delayMicroseconds(half_us);
    }
    dac_silence();
}

static bool enqueue_note(uint16_t freq, uint16_t ms, uint16_t gap) {
    if (!s_note_q) return false;
    const SfxNote note = {freq, ms, gap};
    return xQueueSend(s_note_q, &note, pdMS_TO_TICKS(100)) == pdTRUE;
}

static void audio_task(void* arg) {
    (void)arg;
    SfxNote note;
    for (;;) {
        if (xQueueReceive(s_note_q, &note, portMAX_DELAY) != pdTRUE)
            continue;

        if (!s_on) {
            if (note.gap > 0)
                vTaskDelay(pdMS_TO_TICKS(note.gap));
            continue;
        }

        if (note.freq > 0)
            play_tone(note.freq, note.ms);
        else if (note.ms > 0)
            vTaskDelay(pdMS_TO_TICKS(note.ms));

        if (note.gap > 0)
            vTaskDelay(pdMS_TO_TICKS(note.gap));
    }
}

static void play_phrase(const SfxPhrase* notes, int count) {
    for (int i = 0; i < count; i++)
        enqueue_note(notes[i].freq, notes[i].ms, notes[i].gap);
}

void audio_init() {
    load_pref();
    dac_hw_init();

    if (!s_note_q)
        s_note_q = xQueueCreate(NOTE_QUEUE_LEN, sizeof(SfxNote));

    if (!s_task && s_note_q) {
        if (xTaskCreatePinnedToCore(audio_task, "audio", 3072, nullptr, 5, &s_task, 1) != pdPASS)
            Serial.println("[AUDIO] task falhou");
    }
}

bool audio_on() {
    return s_on;
}

void audio_toggle() {
    s_on = !s_on;
    save_pref();
    if (!s_on)
        audio_stop();
    else if (!s_ready)
        dac_hw_init();
}

void audio_stop() {
    if (s_note_q)
        xQueueReset(s_note_q);
    dac_silence();
}

void audio_tone(uint16_t freq_hz, uint16_t duration_ms) {
    if (!s_on || freq_hz == 0 || duration_ms == 0) return;
    enqueue_note(freq_hz, duration_ms, 0);
}

void audio_simon_tone(int index) {
    if (index < 0 || index > 3) return;
    audio_tone(SIMON_FREQ[index], 180);
}

void audio_play(AudioSfx sfx) {
    if (!s_on) return;

    static const SfxPhrase tick[]      = {{988, 16, 0}};
    static const SfxPhrase select[]    = {{1175, 40, 6}, {988, 30, 0}};
    static const SfxPhrase score[]     = {{784, 35, 8}, {988, 40, 8}, {1175, 50, 8}, {1318, 65, 0}};
    static const SfxPhrase hit[]       = {{1046, 30, 4}, {880, 22, 4}, {1175, 28, 0}};
    static const SfxPhrase drop[]      = {{587, 22, 4}, {494, 28, 4}, {440, 32, 0}};
    static const SfxPhrase level[]     = {{659, 50, 12}, {784, 50, 12}, {988, 50, 12}, {1175, 50, 12}, {1318, 90, 0}};
    static const SfxPhrase line[]      = {{659, 38, 8}, {784, 38, 8}, {988, 42, 8}, {1175, 42, 8}, {1318, 42, 8}, {1568, 110, 0}};
    static const SfxPhrase error[]     = {{622, 85, 12}, {523, 85, 12}, {440, 85, 12}, {370, 110, 0}};
    static const SfxPhrase win[]       = {{784, 55, 10}, {988, 55, 10}, {1175, 55, 10}, {1318, 55, 10}, {1568, 55, 10}, {1760, 160, 0}};
    static const SfxPhrase lose[]      = {{622, 95, 14}, {523, 95, 14}, {440, 95, 14}, {349, 130, 0}};
    static const SfxPhrase record[]    = {{1175, 50, 8}, {1318, 50, 8}, {1568, 50, 8}, {1760, 50, 8}, {1976, 150, 0}};
    static const SfxPhrase startup[]   = {{659, 45, 8}, {784, 45, 8}, {988, 50, 8}, {1175, 55, 8}, {1318, 70, 0}};
    static const SfxPhrase bomb[]      = {{180, 45, 0}, {130, 55, 6}, {90, 65, 6}, {60, 90, 0}};
    static const SfxPhrase flip[]      = {{740, 22, 4}, {880, 28, 0}};
    static const SfxPhrase match[]     = {{988, 38, 6}, {1175, 42, 6}, {1318, 48, 6}, {1568, 70, 0}};
    static const SfxPhrase miss[]      = {{523, 65, 8}, {440, 75, 8}, {370, 85, 0}};
    static const SfxPhrase shoot[]     = {{1175, 28, 4}, {1318, 32, 4}, {1568, 40, 0}};

    switch (sfx) {
    case SFX_TICK:    play_phrase(tick, 1); break;
    case SFX_SELECT:  play_phrase(select, 2); break;
    case SFX_SCORE:   play_phrase(score, 4); break;
    case SFX_HIT:     play_phrase(hit, 3); break;
    case SFX_DROP:    play_phrase(drop, 3); break;
    case SFX_LEVEL:   play_phrase(level, 5); break;
    case SFX_LINE:    play_phrase(line, 6); break;
    case SFX_ERROR:   play_phrase(error, 4); break;
    case SFX_WIN:     play_phrase(win, 6); break;
    case SFX_LOSE:    play_phrase(lose, 4); break;
    case SFX_RECORD:  play_phrase(record, 5); break;
    case SFX_STARTUP: play_phrase(startup, 5); break;
    case SFX_BOMB:    play_phrase(bomb, 4); break;
    case SFX_FLIP:    play_phrase(flip, 2); break;
    case SFX_MATCH:   play_phrase(match, 4); break;
    case SFX_MISS:    play_phrase(miss, 3); break;
    case SFX_SHOOT:   play_phrase(shoot, 3); break;
    }
}
