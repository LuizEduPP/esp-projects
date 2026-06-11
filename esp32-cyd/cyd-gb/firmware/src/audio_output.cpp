#include "audio_output.h"
#include "minigb_apu.h"
#include <Arduino.h>
#include <driver/i2s.h>
#include <string.h>

#ifndef AUDIO_SAMPLE_RATE
#define AUDIO_SAMPLE_RATE 22050
#endif

static bool audio_on = true;
static bool i2s_ready = false;

void audio_output_init() {
    if (i2s_ready) return;

    i2s_config_t cfg = {};
    cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN);
    cfg.sample_rate = AUDIO_SAMPLE_RATE;
    cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
    cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    cfg.dma_buf_count = 4;
    cfg.dma_buf_len = 256;
    cfg.use_apll = false;
    cfg.tx_desc_auto_clear = true;

    if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
        Serial.println("[AUDIO] I2S install failed");
        return;
    }
    i2s_set_pin(I2S_NUM_0, nullptr);

    i2s_set_dac_mode(I2S_DAC_CHANNEL_RIGHT_EN);
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_ready = true;
    Serial.printf("[AUDIO] DAC GPIO%d @ %u Hz\n", SPEAKER_PIN, AUDIO_SAMPLE_RATE);
}

void audio_output_shutdown() {
    if (!i2s_ready) return;
    i2s_zero_dma_buffer(I2S_NUM_0);
    i2s_driver_uninstall(I2S_NUM_0);
    i2s_ready = false;
}

void audio_output_set_enabled(bool on) { audio_on = on; }
bool audio_output_is_enabled() { return audio_on; }

void audio_output_submit(const int16_t* stereo, size_t pairs) {
    if (!i2s_ready || !audio_on || !stereo || pairs == 0) return;

    static uint16_t dac[512 * 2];
    if (pairs > 512) pairs = 512;

    for (size_t i = 0; i < pairs; i++) {
        int32_t m = ((int32_t)stereo[i * 2] + (int32_t)stereo[i * 2 + 1]) / 2;
        m = (m * 5) / 8;
        if (m > 32767) m = 32767;
        if (m < -32768) m = -32768;
        uint16_t v = (uint16_t)(m + 32768);
        dac[i * 2] = 0;
        dac[i * 2 + 1] = v;
    }

    size_t bytes = pairs * 2 * sizeof(uint16_t);
    size_t written = 0;
    i2s_write(I2S_NUM_0, dac, bytes, &written, 0);
}
