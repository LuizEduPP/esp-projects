#include "camera_capture.h"

#include "camera_pins.h"
#include "esp_camera.h"

static camera_fb_t *gFb = nullptr;
static bool gReady = false;
static uint8_t gFailStreak = 0;
static uint32_t gLastShotMs = 0;

static const uint32_t CAPTURE_COOLDOWN_MS = STREAM_INTERVAL_MS - 80;

static void cameraDiscardPending() {
  for (int i = 0; i < 2; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      break;
    }
    esp_camera_fb_return(fb);
  }
}

static bool cameraReinit() {
  if (gReady) {
    esp_camera_deinit();
    gReady = false;
  }
  delay(100);
  return cameraCaptureBegin();
}

bool cameraCaptureBegin() {
  if (gReady) {
    return true;
  }

  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_0;
  cfg.ledc_timer = LEDC_TIMER_0;
  cfg.pin_d0 = CAM_PIN_D0;
  cfg.pin_d1 = CAM_PIN_D1;
  cfg.pin_d2 = CAM_PIN_D2;
  cfg.pin_d3 = CAM_PIN_D3;
  cfg.pin_d4 = CAM_PIN_D4;
  cfg.pin_d5 = CAM_PIN_D5;
  cfg.pin_d6 = CAM_PIN_D6;
  cfg.pin_d7 = CAM_PIN_D7;
  cfg.pin_xclk = CAM_PIN_XCLK;
  cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.pin_vsync = CAM_PIN_VSYNC;
  cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn = CAM_PIN_PWDN;
  cfg.pin_reset = CAM_PIN_RESET;
  cfg.xclk_freq_hz = CAM_XCLK_FREQ_HZ;
  cfg.frame_size = FRAMESIZE_QVGA;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  // Qualidade máxima do encoder OV2640 (0–63, menor = melhor). Sem crop/resize/re-encode
  // para display — compactação e tratamento de imagem ficam só no pc-server.
  cfg.jpeg_quality = 4;
  cfg.fb_count = 2;

  const esp_err_t err = esp_camera_init(&cfg);
  if (err != ESP_OK) {
    Serial.printf("[cam] init falhou 0x%x\n", err);
    return false;
  }

  sensor_t *sensor = esp_camera_sensor_get();
  if (sensor) {
    // Orientação corrigida no pc-server (prepare_display_jpeg).
    sensor->set_vflip(sensor, 0);
    sensor->set_hmirror(sensor, 0);
  }

  gReady = true;
  gFailStreak = 0;
  gLastShotMs = 0;
  Serial.println("[cam] JPEG bruto OV2640 -> server (sem tratamento display)");
  return true;
}

bool cameraCaptureRaw(const uint8_t **outBuf, size_t *outLen) {
  if (!gReady && !cameraCaptureBegin()) {
    return false;
  }

  const uint32_t now = millis();
  if (gLastShotMs != 0 && now - gLastShotMs < CAPTURE_COOLDOWN_MS) {
    return false;
  }

  cameraCaptureRelease();
  cameraDiscardPending();

  gFb = esp_camera_fb_get();
  if (!gFb || gFb->format != PIXFORMAT_JPEG || gFb->len < 2048) {
    Serial.println("[cam] captura falhou");
    cameraCaptureRelease();
    gFailStreak++;
    if (gFailStreak >= 2) {
      Serial.println("[cam] reiniciando driver");
      cameraReinit();
    }
    return false;
  }

  if (gFb->buf[0] != 0xFF || gFb->buf[1] != 0xD8) {
    Serial.println("[cam] JPEG invalido");
    cameraCaptureRelease();
    return false;
  }

  gFailStreak = 0;
  gLastShotMs = now;
  *outBuf = gFb->buf;
  *outLen = gFb->len;
  Serial.printf("[cam] raw %ux%u %u B\n",
                gFb->width, gFb->height, static_cast<unsigned>(gFb->len));
  return true;
}

void cameraCaptureRelease() {
  if (gFb) {
    esp_camera_fb_return(gFb);
    gFb = nullptr;
  }
}

bool cameraCaptureReady() {
  return gReady;
}
