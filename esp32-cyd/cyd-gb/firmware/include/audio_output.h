#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

// Amplificador PAM8002A onboard — GPIO 26 (JST P4)
#define SPEAKER_PIN 26

void audio_output_init();
void audio_output_shutdown();
void audio_output_set_enabled(bool on);
bool audio_output_is_enabled();
void audio_output_submit(const int16_t* stereo_interleaved, size_t sample_pairs);
