#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_camera.h>

#include "audio_capture.h"
#include "pins.h"

#ifndef WIFI_SSID
#define WIFI_SSID "SUA_REDE_WIFI"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "SUA_SENHA_WIFI"
#endif
#ifndef FOLIO_BRAIN_URL
#define FOLIO_BRAIN_URL "http://192.168.1.100:8770"
#endif
#ifndef FOLIO_DEVICE_ID
#define FOLIO_DEVICE_ID "folio-s3-01"
#endif

static const uint32_t FRAME_INTERVAL_MS = 60000;
static const uint32_t MOTION_DEBOUNCE_MS = 30000;

static WebServer gServer(80);
static portMUX_TYPE gCamMux = portMUX_INITIALIZER_UNLOCKED;
static bool gCamOk = false;
static bool gAudioOk = false;
static bool gPaused = false;
static uint32_t gAudioSeq = 0;
static uint32_t gAudioPushOk = 0;
static uint32_t gAudioPushFail = 0;
static uint32_t gFramePushOk = 0;
static uint32_t gFramePushFail = 0;
static unsigned long gBootMs = 0;
static unsigned long gLastFrameMs = 0;
static unsigned long gLastMotionMs = 0;
static int gLastMotionLevel = 0;

static int16_t gPcmBuf[FOLIO_CHUNK_SAMPLES];

static void cors() {
  gServer.sendHeader("Access-Control-Allow-Origin", "*");
}

static void logReq(const char *path) {
  Serial.printf("[http] %s %s | client=%s | uptime=%lums\n",
                gServer.method() == HTTP_GET ? "GET" : "POST", path,
                gServer.client().remoteIP().toString().c_str(), millis() - gBootMs);
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
  c.frame_size = FRAMESIZE_QVGA;
  c.pixel_format = PIXFORMAT_JPEG;
  c.grab_mode = CAMERA_GRAB_LATEST;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  return esp_camera_init(&c) == ESP_OK;
}

static camera_fb_t *captureFrame() {
  portENTER_CRITICAL(&gCamMux);
  camera_fb_t *fb = esp_camera_fb_get();
  portEXIT_CRITICAL(&gCamMux);
  return fb;
}

static bool brainPost(const char *path, const uint8_t *body, size_t len,
                      const char *contentType, const char *extraHeaders) {
  HTTPClient http;
  String url = String(FOLIO_BRAIN_URL) + path;
  if (!http.begin(url)) {
    return false;
  }
  http.setTimeout(15000);
  http.addHeader("Content-Type", contentType);
  http.addHeader("X-Folio-Device-Id", FOLIO_DEVICE_ID);
  if (extraHeaders) {
    http.addHeader("X-Folio-Meta", extraHeaders);
  }
  const int code = http.POST(const_cast<uint8_t *>(body), len);
  http.end();
  return code >= 200 && code < 300;
}

static void pushAudioChunk() {
  if (!gAudioOk || gPaused) {
    return;
  }
  if (!audioReadChunk(gPcmBuf, FOLIO_CHUNK_SAMPLES)) {
    Serial.println("[push] audio read failed");
    gAudioPushFail++;
    return;
  }

  const uint32_t seq = gAudioSeq++;
  const unsigned long tsMs = millis();
  char meta[96];
  snprintf(meta, sizeof(meta), "seq=%lu;ts_ms=%lu;rate=%d", seq, tsMs, FOLIO_SAMPLE_RATE);

  const bool ok = brainPost("/ingest/audio", reinterpret_cast<const uint8_t *>(gPcmBuf),
                            FOLIO_CHUNK_BYTES, "application/octet-stream", meta);
  if (ok) {
    gAudioPushOk++;
    if (seq % 30 == 0) {
      Serial.printf("[push] audio seq=%lu ok=%lu fail=%lu\n", seq, gAudioPushOk,
                    gAudioPushFail);
    }
  } else {
    gAudioPushFail++;
    Serial.printf("[push] audio seq=%lu HTTP fail\n", seq);
  }
}

static void pushFrame(const char *reason) {
  if (!gCamOk || gPaused) {
    return;
  }
  camera_fb_t *fb = captureFrame();
  if (!fb) {
    gFramePushFail++;
    Serial.println("[push] frame capture failed");
    return;
  }

  char meta[128];
  snprintf(meta, sizeof(meta), "reason=%s;ts_ms=%lu", reason, millis());

  const bool ok =
      brainPost("/ingest/frame", fb->buf, fb->len, "image/jpeg", meta);
  esp_camera_fb_return(fb);

  if (ok) {
    gFramePushOk++;
    Serial.printf("[push] frame %s ok=%lu\n", reason, gFramePushOk);
  } else {
    gFramePushFail++;
    Serial.printf("[push] frame %s fail\n", reason);
  }
  gLastFrameMs = millis();
}

static void pushEvent(const char *kind, const char *payload) {
  char body[256];
  snprintf(body, sizeof(body), "{\"kind\":\"%s\",\"payload\":%s,\"ts_ms\":%lu}", kind,
           payload ? payload : "{}", millis());
  if (!brainPost("/ingest/event", reinterpret_cast<const uint8_t *>(body), strlen(body),
                 "application/json", nullptr)) {
    Serial.printf("[push] event %s failed\n", kind);
  }
}

static void pollMotion() {
#if PIN_MOTION >= 0
  const int level = digitalRead(PIN_MOTION);
  if (level == HIGH && level != gLastMotionLevel) {
    const unsigned long now = millis();
    if (now - gLastMotionMs >= MOTION_DEBOUNCE_MS) {
      gLastMotionMs = now;
      pushEvent("motion", "{\"active\":true}");
      pushFrame("motion");
    }
  }
  gLastMotionLevel = level;
#endif
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
  j += "\",\"brain\":\"";
  j += FOLIO_BRAIN_URL;
  j += "\",\"camera\":";
  j += gCamOk ? "true" : "false";
  j += ",\"audio\":";
  j += gAudioOk ? "true" : "false";
  j += ",\"paused\":";
  j += gPaused ? "true" : "false";
  j += ",\"audio_seq\":";
  j += gAudioSeq;
  j += ",\"audio_push_ok\":";
  j += gAudioPushOk;
  j += ",\"audio_push_fail\":";
  j += gAudioPushFail;
  j += ",\"frame_push_ok\":";
  j += gFramePushOk;
  j += ",\"heap\":";
  j += ESP.getFreeHeap();
  j += "}";
  gServer.send(200, "application/json", j);
}

static void handlePause() {
  logReq("/pause");
  if (gServer.method() != HTTP_POST) {
    gServer.send(405, "text/plain", "POST");
    return;
  }
  gPaused = true;
  pushEvent("pause", "{\"active\":true}");
  cors();
  gServer.send(200, "application/json", "{\"ok\":true,\"paused\":true}");
}

static void handleResume() {
  logReq("/resume");
  if (gServer.method() != HTTP_POST) {
    gServer.send(405, "text/plain", "POST");
    return;
  }
  gPaused = false;
  pushEvent("resume", "{\"active\":true}");
  cors();
  gServer.send(200, "application/json", "{\"ok\":true,\"paused\":false}");
}

void setup() {
  Serial.begin(115200);
  delay(300);
  gBootMs = millis();
  Serial.println("\n[boot] ========== folio-node ==========");
  Serial.printf("[boot] device=%s brain=%s\n", FOLIO_DEVICE_ID, FOLIO_BRAIN_URL);
  Serial.printf("[boot] heap=%u psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());

#if PIN_MOTION >= 0
  pinMode(PIN_MOTION, INPUT);
  Serial.printf("[boot] motion GPIO%d\n", PIN_MOTION);
#endif

  gAudioOk = audioBegin();
  gCamOk = cameraBegin();
  Serial.printf("[boot] audio %s | camera %s\n", gAudioOk ? "OK" : "FALHOU",
                gCamOk ? "OK" : "FALHOU");

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[boot] ERRO wifi");
    return;
  }
  Serial.printf("[boot] IP %s rssi=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.println("[boot] push -> brain /ingest/* | local GET /capture /health");
  Serial.println("[boot] =====================================");

  pushEvent("boot", "{\"ok\":true}");

  gServer.on("/capture", HTTP_GET, handleCapture);
  gServer.on("/health", HTTP_GET, handleHealth);
  gServer.on("/pause", HTTP_POST, handlePause);
  gServer.on("/resume", HTTP_POST, handleResume);
  gServer.begin();

  gLastFrameMs = millis();
}

void loop() {
  gServer.handleClient();
  pollMotion();

  if (gAudioOk) {
    pushAudioChunk();
  }

  const unsigned long now = millis();
  if (gCamOk && now - gLastFrameMs >= FRAME_INTERVAL_MS) {
    pushFrame("interval");
  }

  delay(10);
}
