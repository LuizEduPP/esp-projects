#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_task_wdt.h>
#include <cmath>
#include <cstring>
#include <esp_camera.h>

#include "audio_capture.h"
#include "folio_config.h"
#include "pins.h"

#ifndef FOLIO_BRAIN_URL
#define FOLIO_BRAIN_URL "http://192.168.1.28:8770"
#endif
#ifndef FOLIO_DEVICE_ID
#define FOLIO_DEVICE_ID "folio-s3-01"
#endif
#ifndef WIFI_SSID
#define WIFI_SSID "SUA_REDE_WIFI"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "SUA_SENHA_WIFI"
#endif

static WebServer gServer(80);
static portMUX_TYPE gCamMux = portMUX_INITIALIZER_UNLOCKED;
static bool gCamOk = false;
static bool gAudioOk = false;
static bool gWifiOk = false;
static uint32_t gAudioSeq = 0;
static uint32_t gFrameSeq = 0;
static uint32_t gAudioPushOk = 0;
static uint32_t gAudioPushFail = 0;
static uint32_t gFramePushOk = 0;
static uint32_t gFramePushFail = 0;
static unsigned long gBootMs = 0;
static unsigned long gLastFrameMs = 0;
static unsigned long gPushBackoffUntilMs = 0;
static uint32_t gPushBackoffMs = 0;

static int16_t gPcmBuf[FOLIO_CHUNK_SAMPLES];

static void cors() { gServer.sendHeader("Access-Control-Allow-Origin", "*"); }

static void logReq(const char *path) {
  Serial.printf("[http] %s %s | client=%s | uptime=%lums\n",
                gServer.method() == HTTP_GET ? "GET" : "POST", path,
                gServer.client().remoteIP().toString().c_str(), millis() - gBootMs);
}

static bool wifiConnect(uint32_t timeoutMs) {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  Serial.printf("[wifi] connecting to %s…\n", WIFI_SSID);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  const unsigned long deadline = millis() + timeoutMs;
  while (millis() < deadline && WiFi.status() != WL_CONNECTED) {
    delay(200);
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("[wifi] up ip=%s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    return true;
  }
  Serial.println("[wifi] connect failed");
  return false;
}

static void wifiMaintain() {
  static unsigned long lastTry = 0;
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }
  const unsigned long now = millis();
  if (now - lastTry < FOLIO_WIFI_RETRY_MS) {
    return;
  }
  lastTry = now;
  wifiConnect(FOLIO_WIFI_RETRY_MS);
}

static bool cameraBegin() {
  camera_config_t c = {};
  c.ledc_channel = CAM_LEDC_CHANNEL;
  c.ledc_timer = CAM_LEDC_TIMER;
  c.pin_d0 = CAM_PIN_D0;
  c.pin_d1 = CAM_PIN_D1;
  c.pin_d2 = CAM_PIN_D2;
  c.pin_d3 = CAM_PIN_D3;
  c.pin_d4 = CAM_PIN_D4;
  c.pin_d5 = CAM_PIN_D5;
  c.pin_d6 = CAM_PIN_D6;
  c.pin_d7 = CAM_PIN_D7;
  c.pin_xclk = CAM_PIN_XCLK;
  c.pin_pclk = CAM_PIN_PCLK;
  c.pin_vsync = CAM_PIN_VSYNC;
  c.pin_href = CAM_PIN_HREF;
  c.pin_sccb_sda = CAM_PIN_SIOD;
  c.pin_sccb_scl = CAM_PIN_SIOC;
  c.pin_pwdn = CAM_PIN_PWDN;
  c.pin_reset = CAM_PIN_RESET;
  c.xclk_freq_hz = 20000000;
  c.frame_size = (framesize_t)FOLIO_FRAME_SIZE_ID;
  c.pixel_format = PIXFORMAT_JPEG;
  c.grab_mode = CAMERA_GRAB_LATEST;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.jpeg_quality = FOLIO_JPEG_QUALITY;
  c.fb_count = 2;
  return esp_camera_init(&c) == ESP_OK;
}

static camera_fb_t *captureFrame() {
  portENTER_CRITICAL(&gCamMux);
  camera_fb_t *fb = esp_camera_fb_get();
  portEXIT_CRITICAL(&gCamMux);
  return fb;
}

static bool canPushNow() { return millis() >= gPushBackoffUntilMs; }

static void onPushFail() {
  gPushBackoffMs = min<uint32_t>(gPushBackoffMs + 2000UL, (uint32_t)FOLIO_PUSH_BACKOFF_MAX_MS);
  gPushBackoffUntilMs = millis() + gPushBackoffMs;
}

static void onPushOk() {
  gPushBackoffMs = 0;
  gPushBackoffUntilMs = 0;
}

static bool brainPost(const char *path, const uint8_t *body, size_t len,
                      const char *contentType, const char *extraHeaders) {
  if (!gWifiOk) {
    return false;
  }
  HTTPClient http;
  String url = String(FOLIO_BRAIN_URL) + path;
  if (!http.begin(url)) {
    Serial.printf("[push] HTTP begin failed %s\n", url.c_str());
    return false;
  }
  http.setTimeout(15000);
  http.addHeader("Content-Type", contentType);
  http.addHeader("X-Folio-Device-Id", FOLIO_DEVICE_ID);
  if (extraHeaders) {
    http.addHeader("X-Folio-Meta", extraHeaders);
  }
  const int code = http.POST(const_cast<uint8_t *>(body), len);
  if (code < 200 || code >= 300) {
    Serial.printf("[push] HTTP %d %s\n", code, path);
  }
  http.end();
  return code >= 200 && code < 300;
}

static bool trySendAudio(uint32_t seq, const int16_t *pcm, const char *meta) {
  if (!canPushNow()) {
    return false;
  }
  const bool ok =
      brainPost("/ingest/audio", reinterpret_cast<const uint8_t *>(pcm), FOLIO_CHUNK_BYTES,
                "application/octet-stream", meta);
  if (ok) {
    onPushOk();
    gAudioPushOk++;
    if (seq < 3 || seq % 30 == 0) {
      Serial.printf("[push] audio seq=%lu ok=%lu\n", seq, gAudioPushOk);
    }
    return true;
  }
  onPushFail();
  gAudioPushFail++;
  return false;
}

static bool trySendFrame(uint32_t id, const uint8_t *jpeg, size_t len, const char *meta) {
  if (!canPushNow()) {
    return false;
  }
  const bool ok = brainPost("/ingest/frame", jpeg, len, "image/jpeg", meta);
  if (ok) {
    onPushOk();
    gFramePushOk++;
    Serial.printf("[push] frame id=%lu ok=%lu\n", id, gFramePushOk);
    return true;
  }
  onPushFail();
  gFramePushFail++;
  return false;
}

static uint16_t pcmPeak(const int16_t *pcm, size_t count) {
  uint16_t peak = 0;
  for (size_t i = 0; i < count; ++i) {
    const int16_t a = pcm[i] < 0 ? -pcm[i] : pcm[i];
    if (a > peak) {
      peak = static_cast<uint16_t>(a);
    }
  }
  return peak;
}

static float pcmEnergy(const int16_t *pcm, size_t count) {
  if (count == 0) {
    return 0.f;
  }
  double sum = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double n = pcm[i] / 32768.0;
    sum += n * n;
  }
  return static_cast<float>(sqrt(sum / count));
}

static bool audioWorthCapture(const int16_t *pcm, size_t count, uint16_t *outPeak,
                              float *outEnergy) {
  const uint16_t peak = pcmPeak(pcm, count);
  const float energy = pcmEnergy(pcm, count);
  if (outPeak) {
    *outPeak = peak;
  }
  if (outEnergy) {
    *outEnergy = energy;
  }
  if (peak == 0 || peak >= 30000) {
    return false;
  }
  return energy >= FOLIO_SPEECH_ENERGY;
}

static void captureAudioChunk() {
  if (!gAudioOk || !audioReadChunk(gPcmBuf, FOLIO_CHUNK_SAMPLES)) {
    return;
  }

  const uint32_t seq = gAudioSeq++;
  uint16_t peak = 0;
  float energy = 0.f;
  if (!audioWorthCapture(gPcmBuf, FOLIO_CHUNK_SAMPLES, &peak, &energy)) {
    if (seq < 5 || seq % 30 == 0) {
      Serial.printf("[capture] audio quiet peak=%u energy=%.4f\n", peak, energy);
    }
    return;
  }

  if (!gWifiOk) {
    return;
  }

  char meta[128];
  snprintf(meta, sizeof(meta), "seq=%lu;ts_ms=%lu;rate=%d;energy=%.4f", seq, millis(),
           FOLIO_SAMPLE_RATE, energy);
  trySendAudio(seq, gPcmBuf, meta);
}

static void captureFrameChunk() {
  if (!gCamOk) {
    return;
  }
  camera_fb_t *fb = captureFrame();
  if (!fb) {
    Serial.println("[capture] frame failed");
    return;
  }

  const uint32_t id = gFrameSeq++;
  char meta[96];
  snprintf(meta, sizeof(meta), "reason=interval;ts_ms=%lu", millis());

  if (gWifiOk) {
    trySendFrame(id, fb->buf, fb->len, meta);
  }

  gLastFrameMs = millis();
  esp_camera_fb_return(fb);
}

static void handleCapture() {
  logReq("/capture");
  camera_fb_t *fb = captureFrame();
  if (!fb) {
    gServer.send(503, "text/plain", "busy");
    return;
  }
  cors();
  gServer.send_P(200, "image/jpeg", reinterpret_cast<const char *>(fb->buf), fb->len);
  esp_camera_fb_return(fb);
}

static void handleHealth() {
  logReq("/health");
  cors();
  String j = "{\"device_id\":\"";
  j += FOLIO_DEVICE_ID;
  j += "\",\"ip\":\"";
  j += WiFi.localIP().toString();
  j += "\",\"wifi\":";
  j += gWifiOk ? "true" : "false";
  j += ",\"brain\":\"";
  j += FOLIO_BRAIN_URL;
  j += "\",\"camera\":";
  j += gCamOk ? "true" : "false";
  j += ",\"audio\":";
  j += gAudioOk ? "true" : "false";
  j += ",\"audio_push_ok\":";
  j += gAudioPushOk;
  j += ",\"frame_push_ok\":";
  j += gFramePushOk;
  j += ",\"heap\":";
  j += ESP.getFreeHeap();
  j += "}";
  gServer.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  gBootMs = millis();
  Serial.println("\n[boot] folio-node (basic)");
  Serial.printf("[boot] device=%s brain=%s\n", FOLIO_DEVICE_ID, FOLIO_BRAIN_URL);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);

  gCamOk = cameraBegin();
  gAudioOk = audioBegin();
  Serial.printf("[boot] audio %s | camera %s\n", gAudioOk ? "OK" : "FAIL",
                gCamOk ? "OK" : "FAIL");

  gWifiOk = wifiConnect(15000);
  if (!gWifiOk) {
    Serial.println("[boot] wifi offline — will retry");
  }

  gServer.on("/capture", HTTP_GET, handleCapture);
  gServer.on("/health", HTTP_GET, handleHealth);
  gServer.begin();

  gLastFrameMs = millis();
}

void loop() {
  gServer.handleClient();
  wifiMaintain();
  gWifiOk = WiFi.status() == WL_CONNECTED;

  const unsigned long now = millis();

  static unsigned long lastHeartbeat = 0;
  if (now - lastHeartbeat >= FOLIO_STATUS_INTERVAL_MS) {
    lastHeartbeat = now;
    Serial.printf("[status] wifi=%s audio_ok=%lu frame_ok=%lu heap=%u\n", gWifiOk ? "up" : "down",
                  gAudioPushOk, gFramePushOk, ESP.getFreeHeap());
  }

  if (gAudioOk) {
    captureAudioChunk();
  }

  if (gCamOk && now - gLastFrameMs >= FOLIO_FRAME_INTERVAL_MS) {
    captureFrameChunk();
  }

  esp_task_wdt_reset();
  delay(10);
}
