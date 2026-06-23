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
    SFX_BOMB,
    SFX_FLIP,
    SFX_MATCH,
    SFX_MISS,
    SFX_SHOOT,
} AudioSfx;

void audio_init();
void audio_stop();
void audio_tone(uint16_t freq_hz, uint16_t duration_ms);
void audio_play(AudioSfx sfx);
void audio_simon_tone(int index);
bool audio_on();
void audio_toggle();
