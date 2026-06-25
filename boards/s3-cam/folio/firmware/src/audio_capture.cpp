#include "audio_capture.h"

#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include <math.h>

#include "pins.h"

enum class ReadMode : uint8_t { Bits16, Bits32 };

static bool gAudioOk = false;
static bool gUseRightChannel = false;
static bool gStereoSlots = false;
static ReadMode gReadMode = ReadMode::Bits32;
static int gSampleShift = 11;

static bool gSavedValid = false;
static i2s_bits_per_sample_t gSavedBits = I2S_BITS_PER_SAMPLE_32BIT;
static i2s_channel_fmt_t gSavedFmt = I2S_CHANNEL_FMT_ONLY_LEFT;
static int gSavedBck = PIN_I2S_SCK;
static int gSavedWs = PIN_I2S_WS;
static int gSavedDout = PIN_I2S_DOUT;
static bool gSavedRight = false;

static int gActiveDout = PIN_I2S_DOUT;
static bool gDoutStuck = false;

static const int kDoutSafePins[] = {21, 14};

static void releaseMicPins() {
  gpio_reset_pin(static_cast<gpio_num_t>(PIN_I2S_WS));
  gpio_reset_pin(static_cast<gpio_num_t>(PIN_I2S_SCK));
  gpio_reset_pin(static_cast<gpio_num_t>(PIN_I2S_DOUT));
  for (int pin : kDoutSafePins) {
    gpio_reset_pin(static_cast<gpio_num_t>(pin));
  }
}

static bool rawLineStuck(const int32_t *raw, size_t count) {
  if (count == 0) {
    return true;
  }
  size_t stuck = 0;
  for (size_t i = 0; i < count; ++i) {
    if (raw[i] == -1 || raw[i] == 0) {
      stuck++;
    }
  }
  return stuck * 10 >= count * 9;
}

static int16_t clampI16(int32_t sample) {
  if (sample > 32767) {
    return 32767;
  }
  if (sample < -32768) {
    return -32768;
  }
  return static_cast<int16_t>(sample);
}

static void audioStop() {
  if (!gAudioOk) {
    return;
  }
  i2s_driver_uninstall(I2S_NUM_0);
  gAudioOk = false;
}

static bool readRawBytes(uint8_t *out, size_t bytesWanted) {
  size_t total = 0;
  while (total < bytesWanted) {
    size_t chunk = 0;
    if (i2s_read(I2S_NUM_0, out + total, bytesWanted - total, &chunk, portMAX_DELAY) != ESP_OK ||
        chunk == 0) {
      return false;
    }
    total += chunk;
  }
  return true;
}

static bool readRaw32(int32_t *out, uint32_t count) {
  return readRawBytes(reinterpret_cast<uint8_t *>(out), count * sizeof(int32_t));
}

static bool readRaw16(int16_t *out, uint32_t count) {
  return readRawBytes(reinterpret_cast<uint8_t *>(out), count * sizeof(int16_t));
}

static void discardSamples(uint32_t count) {
  if (gReadMode == ReadMode::Bits16) {
    int16_t scratch[256];
    while (count > 0) {
      const uint32_t n = count > 256U ? 256U : count;
      if (!readRaw16(scratch, n)) {
        break;
      }
      count -= n;
    }
    return;
  }
  int32_t scratch[128];
  while (count > 0) {
    const uint32_t n = count > 128U ? 128U : count;
    if (!readRaw32(scratch, n)) {
      break;
    }
    count -= n;
  }
}

struct LevelStats {
  int32_t rawPeak;
  int32_t pcmPeak;
  float rms;
};

static LevelStats measureFromPcm(const int16_t *pcm, size_t count) {
  LevelStats st = {};
  for (size_t i = 0; i < count; ++i) {
    const int32_t s = pcm[i];
    const int32_t absPcm = s < 0 ? -s : s;
    if (absPcm > st.pcmPeak) {
      st.pcmPeak = absPcm;
    }
    st.rms += static_cast<float>(s) * static_cast<float>(s);
  }
  st.rms = sqrtf(st.rms / static_cast<float>(count)) / 32768.f;
  return st;
}

static LevelStats measureMono32(const int32_t *raw, size_t count) {
  LevelStats st = {};
  for (size_t i = 0; i < count; ++i) {
    const int32_t absRaw = raw[i] < 0 ? -raw[i] : raw[i];
    if (absRaw > st.rawPeak) {
      st.rawPeak = absRaw;
    }
    const int32_t s = raw[i] >> gSampleShift;
    const int32_t absPcm = s < 0 ? -s : s;
    if (absPcm > st.pcmPeak) {
      st.pcmPeak = absPcm;
    }
    st.rms += static_cast<float>(s) * static_cast<float>(s);
  }
  st.rms = sqrtf(st.rms / static_cast<float>(count)) / 32768.f;
  return st;
}

static LevelStats measureLevel() {
  if (gReadMode == ReadMode::Bits16) {
    int16_t pcm[256];
    if (!readRaw16(pcm, 256)) {
      return {};
    }
    if (gStereoSlots) {
      int16_t mono[128];
      for (size_t i = 0; i < 128; ++i) {
        mono[i] = gUseRightChannel ? pcm[i * 2 + 1] : pcm[i * 2];
      }
      return measureFromPcm(mono, 128);
    }
    return measureFromPcm(pcm, 256);
  }

  int32_t raw[256];
  if (!readRaw32(raw, 256)) {
    return {};
  }
  if (gStereoSlots) {
    LevelStats left = {};
    LevelStats right = {};
    for (size_t i = 0; i < 128; ++i) {
      const int32_t l = raw[i * 2];
      const int32_t r = raw[i * 2 + 1];
      const int32_t absL = l < 0 ? -l : l;
      const int32_t absR = r < 0 ? -r : r;
      if (absL > left.rawPeak) {
        left.rawPeak = absL;
      }
      if (absR > right.rawPeak) {
        right.rawPeak = absR;
      }
      const int32_t sl = l >> gSampleShift;
      const int32_t sr = r >> gSampleShift;
      const int32_t absSl = sl < 0 ? -sl : sl;
      const int32_t absSr = sr < 0 ? -sr : sr;
      if (absSl > left.pcmPeak) {
        left.pcmPeak = absSl;
      }
      if (absSr > right.pcmPeak) {
        right.pcmPeak = absSr;
      }
      left.rms += static_cast<float>(sl) * static_cast<float>(sl);
      right.rms += static_cast<float>(sr) * static_cast<float>(sr);
    }
    left.rms = sqrtf(left.rms / 128.f) / 32768.f;
    right.rms = sqrtf(right.rms / 128.f) / 32768.f;
    return gUseRightChannel ? right : left;
  }
  return measureMono32(raw, 256);
}

static void logRawSamples() {
  if (gReadMode == ReadMode::Bits16) {
    int16_t pcm[8];
    if (!readRaw16(pcm, 8)) {
      return;
    }
    Serial.printf("[audio] raw16[0..3]=%d %d %d %d\n", pcm[0], pcm[1], pcm[2], pcm[3]);
    return;
  }
  int32_t raw[4];
  if (!readRaw32(raw, 4)) {
    return;
  }
  Serial.printf("[audio] raw32[0..3]=%ld %ld %ld %ld\n", raw[0], raw[1], raw[2], raw[3]);
}

static bool installI2s(i2s_bits_per_sample_t bits, i2s_channel_fmt_t channelFmt, int bckPin,
                       int wsPin, int sdPin) {
  audioStop();
  releaseMicPins();

  i2s_config_t cfg = {};
  cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate = FOLIO_SAMPLE_RATE;
  cfg.bits_per_sample = bits;
  cfg.channel_format = channelFmt;
  cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count = 8;
  cfg.dma_buf_len = 512;
  cfg.use_apll = false;
  cfg.tx_desc_auto_clear = false;
  cfg.fixed_mclk = 0;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, nullptr) != ESP_OK) {
    return false;
  }

  i2s_pin_config_t pins = {};
  pins.bck_io_num = bckPin;
  pins.ws_io_num = wsPin;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = sdPin;

  if (i2s_set_pin(I2S_NUM_0, &pins) != ESP_OK) {
    i2s_driver_uninstall(I2S_NUM_0);
    return false;
  }

  i2s_zero_dma_buffer(I2S_NUM_0);
  delay(120);
  discardSamples(768);
  gReadMode = bits == I2S_BITS_PER_SAMPLE_16BIT ? ReadMode::Bits16 : ReadMode::Bits32;
  gStereoSlots = channelFmt == I2S_CHANNEL_FMT_RIGHT_LEFT;
  gActiveDout = sdPin;
  gAudioOk = true;
  return true;
}

static void logLevel(const char *label, const LevelStats &st) {
  Serial.printf("[audio] %s rawPeak=%ld pcmPeak=%ld rms=%.4f shift=%d mode=%s\n", label,
                st.rawPeak, st.pcmPeak, st.rms, gSampleShift,
                gReadMode == ReadMode::Bits16 ? "16" : "32");
  if (st.pcmPeak >= 32000) {
    Serial.println("[audio] CLIPPED — check L/R+GND and WS/SCK/DOUT wiring");
  } else if (st.pcmPeak == 0) {
    Serial.println("[audio] ZERO — check L/R+GND and WS/SCK/DOUT wiring");
  }
}

static bool scoreOk(const LevelStats &st) {
  if (st.pcmPeak >= 30000) {
    return false;
  }
  // INMP441 idle: pcmPeak ~100-5000 with shift 11; 16-bit wrong mode stays at 0-10.
  if (st.pcmPeak >= 80 && st.pcmPeak < 30000) {
    return true;
  }
  // Strong raw without clipping — mic alive but room quiet at boot.
  if (st.rawPeak >= 4000 && st.rawPeak < 0x60000000L && st.pcmPeak > 0 && st.pcmPeak < 30000) {
    return true;
  }
  return false;
}

static void saveConfig(i2s_bits_per_sample_t bits, i2s_channel_fmt_t fmt, bool rightChannel,
                       int bckPin, int wsPin, int sdPin) {
  gSavedValid = true;
  gSavedBits = bits;
  gSavedFmt = fmt;
  gSavedRight = rightChannel;
  gSavedBck = bckPin;
  gSavedWs = wsPin;
  gSavedDout = sdPin;
}

static bool tryConfig(i2s_bits_per_sample_t bits, i2s_channel_fmt_t fmt, bool rightChannel,
                      int bckPin, int wsPin, int sdPin, const char *label, bool verbose) {
  gUseRightChannel = rightChannel;
  if (!installI2s(bits, fmt, bckPin, wsPin, sdPin)) {
    return false;
  }

  int32_t probe[32];
  if (readRaw32(probe, 32) && rawLineStuck(probe, 32)) {
    gDoutStuck = true;
    Serial.printf("[audio] DOUT GPIO%d stuck (raw=-1/0) — loose wire or wrong pin\n", sdPin);
    if (sdPin >= 35 && sdPin <= 42) {
      Serial.println("[audio] GOOUUU: GPIO42=MTMS/JTAG — wire mic DOUT to GPIO21 (see PINOUT.md)");
    }
    return false;
  }

  const int shifts[] = {11, 8, 14, 10, 13};
  for (int shift : shifts) {
    if (gReadMode == ReadMode::Bits16) {
      gSampleShift = 0;
    } else {
      gSampleShift = shift;
    }
    const LevelStats st = measureLevel();
    if (verbose) {
      logLevel(label, st);
    }
    if (scoreOk(st)) {
      gDoutStuck = false;
      saveConfig(bits, fmt, rightChannel, bckPin, wsPin, sdPin);
      Serial.printf("[audio] OK DOUT GPIO%d %s shift=%d pcmPeak=%ld\n", sdPin, label, gSampleShift,
                    st.pcmPeak);
      return true;
    }
  }
  return false;
}

static void addUniquePin(int *pins, size_t *count, int pin) {
  for (size_t i = 0; i < *count; ++i) {
    if (pins[i] == pin) {
      return;
    }
  }
  pins[(*count)++] = pin;
}

static bool probeAllDouts() {
  int pins[3] = {};
  size_t n = 0;
  addUniquePin(pins, &n, PIN_I2S_DOUT);
  for (int pin : kDoutSafePins) {
    addUniquePin(pins, &n, pin);
  }

  for (size_t i = 0; i < n; ++i) {
    if (tryConfig(I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_ONLY_LEFT, false, PIN_I2S_SCK,
                  PIN_I2S_WS, pins[i], "32b LEFT", false)) {
      return true;
    }
    esp_task_wdt_reset();
  }
  return false;
}

bool audioDoutStuck() { return gDoutStuck; }

bool audioBegin() {
  gDoutStuck = false;
  Serial.printf("[audio] INMP441 WS=%d SCK=%d DOUT cfg=%d (module pin label: SD)\n",
                PIN_I2S_WS, PIN_I2S_SCK, PIN_I2S_DOUT);
  Serial.println("[audio] safe DOUT: GPIO21 or GPIO14 — never GPIO35-42 (SDIO/JTAG/PSRAM)");
  Serial.println("[audio] tie L/R to mic GND (not floating)");

  if (probeAllDouts()) {
    return true;
  }

  struct Candidate {
    i2s_bits_per_sample_t bits;
    i2s_channel_fmt_t fmt;
    bool right;
    int bck;
    int ws;
    const char *label;
  };

  const Candidate candidates[] = {
      {I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_ONLY_LEFT, false, PIN_I2S_SCK, PIN_I2S_WS,
       "32b LEFT"},
      {I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_ONLY_RIGHT, true, PIN_I2S_SCK, PIN_I2S_WS,
       "32b RIGHT"},
      {I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_RIGHT_LEFT, false, PIN_I2S_SCK, PIN_I2S_WS,
       "32b stereo L"},
      {I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_RIGHT_LEFT, true, PIN_I2S_SCK, PIN_I2S_WS,
       "32b stereo R"},
      {I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_ONLY_LEFT, false, PIN_I2S_WS, PIN_I2S_SCK,
       "32b LEFT swap WS/SCK"},
  };

  for (const Candidate &c : candidates) {
    if (tryConfig(c.bits, c.fmt, c.right, c.bck, c.ws, PIN_I2S_DOUT, c.label, true)) {
      return true;
    }
  }

  gSampleShift = 11;
  if (!installI2s(I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_ONLY_LEFT, PIN_I2S_SCK, PIN_I2S_WS,
                  PIN_I2S_DOUT)) {
    Serial.println("[audio] I2S install failed");
    return false;
  }
  logRawSamples();
  int32_t probe[4];
  if (readRaw32(probe, 4) && rawLineStuck(probe, 4)) {
    Serial.println("[audio] FAIL — DOUT dead (raw=-1); wire mic DOUT to GPIO21");
    gDoutStuck = true;
    audioStop();
    return false;
  }
  const LevelStats st = measureLevel();
  logLevel("FALLBACK 32b LEFT shift=11", st);
  saveConfig(I2S_BITS_PER_SAMPLE_32BIT, I2S_CHANNEL_FMT_ONLY_LEFT, false, PIN_I2S_SCK, PIN_I2S_WS,
             PIN_I2S_DOUT);
  if (st.pcmPeak >= 30000) {
    Serial.println("[audio] FAIL — saturated signal");
    audioStop();
    return false;
  }
  if (st.pcmPeak < 80) {
    Serial.println("[audio] quiet at boot — speak near mic");
  }
  return true;
}

bool audioRecover() {
  if (gDoutStuck) {
    return false;
  }
  Serial.println("[audio] reinit I2S");
  releaseMicPins();
  if (gSavedValid &&
      tryConfig(gSavedBits, gSavedFmt, gSavedRight, gSavedBck, gSavedWs, gSavedDout, "recover",
                false)) {
    return true;
  }
  return audioBegin();
}

void audioEnd() {
  audioStop();
}

bool audioReadChunk(int16_t *out, uint32_t sampleCount) {
  if (!gAudioOk || !out || sampleCount == 0) {
    return false;
  }

  if (gReadMode == ReadMode::Bits16) {
    if (gStereoSlots) {
      int16_t raw[512];
      const uint32_t words = sampleCount * 2;
      if (words > 512 || !readRaw16(raw, words)) {
        return false;
      }
      for (uint32_t i = 0; i < sampleCount; ++i) {
        out[i] = gUseRightChannel ? raw[i * 2 + 1] : raw[i * 2];
      }
      return true;
    }
    return readRaw16(out, sampleCount);
  }

  if (gStereoSlots) {
    int32_t raw[512];
    const uint32_t words = sampleCount * 2;
    if (words > 512 || !readRaw32(raw, words)) {
      return false;
    }
    for (uint32_t i = 0; i < sampleCount; ++i) {
      const int32_t s = gUseRightChannel ? raw[i * 2 + 1] : raw[i * 2];
      out[i] = clampI16(s >> gSampleShift);
    }
    return true;
  }

  int32_t raw[256];
  uint32_t written = 0;
  while (written < sampleCount) {
    const uint32_t block = (sampleCount - written) < 256U ? (sampleCount - written) : 256U;
    if (!readRaw32(raw, block)) {
      return false;
    }
    for (uint32_t i = 0; i < block; ++i) {
      out[written++] = clampI16(raw[i] >> gSampleShift);
    }
  }
  return true;
}
