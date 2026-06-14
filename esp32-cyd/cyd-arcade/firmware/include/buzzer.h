#pragma once
#include <stdint.h>
#include <stdbool.h>

typedef enum {
    SFX_TICK,
    SFX_SELECT,
    SFX_SCORE,
    SFX_HIT,
    SFX_DROP,
    SFX_LEVEL,
    SFX_LINE,
    SFX_ERROR,
    SFX_WIN,
    SFX_LOSE,
    SFX_RECORD,
    SFX_STARTUP,
} BuzzerSfx;

void buzzer_init();
void buzzer_stop();
void buzzer_tone(uint16_t freq_hz, uint16_t duration_ms);
void buzzer_play(BuzzerSfx sfx);
void buzzer_simon_tone(int index);
bool buzzer_sound_on();
void buzzer_sound_toggle();
