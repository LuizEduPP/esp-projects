#include "audio_capture.h"

#include <Arduino.h>
#include <driver/i2s.h>

#include "pins.h"

static bool gAudioOk = false;

bool audioBegin() {
  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = FOLIO_SAMPLE_RATE;
  cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 256;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    Serial.println("[audio] I2S install failed");
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.bck_io_num = PIN_I2S_SCK;
  pins.ws_io_num = PIN_I2S_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = PIN_I2S_SD;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    Serial.println("[audio] I2S pin config failed");
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  gAudioOk = true;
  Serial.println("[audio] INMP441 OK 16kHz mono");
  return true;
}

void audioEnd() {
  if (!gAudioOk) {
    return;
  }
  i2s_driver_uninstall(I2S_NUM_0);
  gAudioOk = false;
}

bool audioReadChunk(int16_t *out, uint32_t sampleCount) {
  if (!gAudioOk || !out || sampleCount == 0) {
    return false;
  }

  size_t bytesRead = 0;
  const size_t bytesWanted = sampleCount * sizeof(int16_t);
  uint8_t *raw = reinterpret_cast<uint8_t *>(out);
  size_t total = 0;

  while (total < bytesWanted) {
    size_t chunk = 0;
    const esp_err_t err =
        i2s_read(I2S_NUM_0, raw + total, bytesWanted - total, &chunk, portMAX_DELAY);
    if (err != ESP_OK || chunk == 0) {
      return false;
    }
    total += chunk;
  }
  bytesRead = total;
  (void)bytesRead;
  return true;
}
