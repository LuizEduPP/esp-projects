#include <Arduino.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_camera.h>

#include "audio_capture.h"
#include "folio_config.h"
#include "node_config.h"
#include "pins.h"
#include "spool_store.h"

#ifndef WIFI_SSID
#define WIFI_SSID "SUA_REDE_WIFI"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "SUA_SENHA_WIFI"
#endif
#ifndef FOLIO_BRAIN_URL
#define FOLIO_BRAIN_URL "http://192.168.1.28:8770"
#endif
#ifndef FOLIO_DEVICE_ID
#define FOLIO_DEVICE_ID "folio-s3-01"
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
static unsigned long gLastWifiTryMs = 0;
static unsigned long gPushBackoffUntilMs = 0;
static uint32_t gPushBackoffMs = 0;
static uint32_t gPushBackoffMaxMs = FOLIO_PUSH_BACKOFF_MAX_MS;

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
  gPushBackoffMs = min<uint32_t>(gPushBackoffMs + 2000UL, gPushBackoffMaxMs);
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
  http.addHeader("X-Folio-Config-Version", nodeConfigVersionHeader());
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
    if (spoolOk()) {
      spoolDeleteAudio(seq);
    }
    if (seq < 3 || seq % 30 == 0) {
      Serial.printf("[push] audio seq=%lu ok=%lu spool=%lu\n", seq, gAudioPushOk,
                    spoolPendingAudio());
    }
    return true;
  }
  onPushFail();
  gAudioPushFail++;
  Serial.printf("[push] audio seq=%lu fail pending=%lu backoff=%lums\n", seq,
                spoolPendingAudio(), gPushBackoffMs);
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
    if (spoolOk()) {
      spoolDeleteFrame(id);
    }
    Serial.printf("[push] frame id=%lu ok=%lu spool=%lu\n", id, gFramePushOk,
                  spoolPendingFrames());
    return true;
  }
  onPushFail();
  gFramePushFail++;
  Serial.printf("[push] frame id=%lu fail (spool=%lu backoff=%lums)\n", id,
                spoolPendingFrames(), gPushBackoffMs);
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

static void captureAudioChunk() {
  if (!gAudioOk) {
    return;
  }
  if (!audioReadChunk(gPcmBuf, FOLIO_CHUNK_SAMPLES)) {
    Serial.println("[capture] audio read failed");
    return;
  }

  const uint32_t seq = gAudioSeq++;
  const uint16_t peak = pcmPeak(gPcmBuf, FOLIO_CHUNK_SAMPLES);
  if (seq < 3 || seq % 30 == 0) {
    Serial.printf("[capture] audio seq=%lu peak=%u\n", seq, peak);
    if (peak == 0) {
      Serial.println("[capture] silence — tie L/R to mic GND");
    } else if (peak >= 32000) {
      Serial.println("[capture] saturated — check L/R+GND and WS/SCK/DOUT");
    }
  }

  char meta[96];
  snprintf(meta, sizeof(meta), "seq=%lu;ts_ms=%lu;rate=%d", seq, millis(), FOLIO_SAMPLE_RATE);

  const bool signalOk = peak >= 100 && peak < 30000;
  if (!signalOk && (seq < 5 || seq % 30 == 0)) {
    if (peak < 100) {
      Serial.println("[capture] silence — speak near mic or check wiring");
    } else {
      Serial.println("[capture] chunk skipped — invalid signal (not pushed)");
    }
  }

  static uint8_t lowPeakStreak = 0;
  if (!audioDoutStuck() && peak < 50) {
    if (++lowPeakStreak >= 30) {
      lowPeakStreak = 0;
      gAudioOk = audioRecover();
    }
  } else {
    lowPeakStreak = 0;
  }

  if (!spoolSaveAudio(seq, gPcmBuf, meta)) {
    Serial.printf("[microsd] FAIL audio seq=%lu — card required before push\n", seq);
    return;
  }
  if (seq < 3 || seq % 30 == 0) {
    Serial.printf("[microsd] audio seq=%lu saved pending=%lu\n", seq, spoolPendingAudio());
  }

  if (signalOk && gWifiOk) {
    trySendAudio(seq, gPcmBuf, meta);
  }
}

static void captureFrameChunk(const char *reason) {
  if (!gCamOk) {
    return;
  }
  camera_fb_t *fb = captureFrame();
  if (!fb) {
    Serial.println("[capture] frame failed");
    return;
  }

  const uint32_t id = gFrameSeq++;
  char meta[128];
  snprintf(meta, sizeof(meta), "reason=%s;ts_ms=%lu", reason, millis());

  if (!spoolSaveFrame(id, fb->buf, fb->len, meta)) {
    Serial.printf("[microsd] FAIL frame id=%lu — card required before push\n", id);
    esp_camera_fb_return(fb);
    return;
  }
  Serial.printf("[microsd] frame id=%lu saved pending=%lu\n", id, spoolPendingFrames());

  if (gWifiOk) {
    trySendFrame(id, fb->buf, fb->len, meta);
  }

  esp_camera_fb_return(fb);
  gLastFrameMs = millis();
}

static void drainSpoolOnce() {
  if (!gWifiOk || !canPushNow()) {
    return;
  }
  if (!spoolOk()) {
    return;
  }

  uint32_t seq = 0;
  char meta[128];
  if (spoolOldestAudio(&seq, meta, sizeof(meta))) {
    if (spoolReadAudio(seq, gPcmBuf, meta, sizeof(meta))) {
      trySendAudio(seq, gPcmBuf, meta);
      return;
    }
    Serial.printf("[spool] corrupt audio seq=%lu — dropping\n", seq);
    spoolDeleteAudio(seq);
    return;
  }

  uint32_t id = 0;
  uint8_t *jpeg = nullptr;
  size_t len = 0;
  if (spoolOldestFrame(&id, meta, sizeof(meta)) &&
      spoolReadFrame(id, &jpeg, &len, meta, sizeof(meta))) {
    trySendFrame(id, jpeg, len, meta);
    spoolFreeBuffer(jpeg);
  }
}

static void pushEvent(const char *kind, const char *payload) {
  if (!gWifiOk || !canPushNow()) {
    return;
  }
  char body[256];
  snprintf(body, sizeof(body), "{\"kind\":\"%s\",\"payload\":%s,\"ts_ms\":%lu}", kind,
           payload ? payload : "{}", millis());
  if (!brainPost("/ingest/event", reinterpret_cast<const uint8_t *>(body), strlen(body),
                 "application/json", nullptr)) {
    Serial.printf("[push] event %s failed\n", kind);
  }
}

static void ensureWifi() {
  const uint32_t wifiRetryMs = nodeConfig().wifiRetryMs;
  if (WiFi.status() == WL_CONNECTED) {
    if (!gWifiOk) {
      gWifiOk = true;
      Serial.printf("[wifi] up %s rssi=%d\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    }
    return;
  }

  gWifiOk = false;
  const unsigned long now = millis();
  if (now - gLastWifiTryMs < wifiRetryMs) {
    return;
  }
  gLastWifiTryMs = now;
  Serial.println("[wifi] reconnecting…");
  WiFi.reconnect();
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
  j += "\",\"spool\":";
  j += spoolOk() ? "true" : "false";
  j += ",\"spool_audio\":";
  j += spoolOk() ? spoolPendingAudio() : 0;
  j += ",\"spool_frames\":";
  j += spoolOk() ? spoolPendingFrames() : 0;
  j += ",\"camera\":";
  j += gCamOk ? "true" : "false";
  j += ",\"audio\":";
  j += gAudioOk ? "true" : "false";
  j += ",\"audio_seq\":";
  j += gAudioSeq;
  j += ",\"audio_push_ok\":";
  j += gAudioPushOk;
  j += ",\"audio_push_fail\":";
  j += gAudioPushFail;
  j += ",\"frame_push_ok\":";
  j += gFramePushOk;
  j += ",\"config_version\":\"";
  j += nodeConfigVersionHeader();
  j += "\",\"heap\":";
  j += ESP.getFreeHeap();
  j += "}";
  gServer.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  gBootMs = millis();
  Serial.println("\n[boot] ========== folio-node ==========");
  Serial.printf("[boot] device=%s brain=%s\n", FOLIO_DEVICE_ID, FOLIO_BRAIN_URL);
  Serial.printf("[boot] heap=%u psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());

  gCamOk = cameraBegin();
  const bool microsdOk = spoolBegin();
  gAudioOk = audioBegin();
  Serial.printf("[boot] pipeline: microSD -> push brain | card %s pending a=%lu f=%lu\n",
                microsdOk ? "OK" : "FAIL", spoolPendingAudio(), spoolPendingFrames());
  if (spoolRequired() && !microsdOk) {
    Serial.println("[boot] WARN: FAT32 microSD required — insert before power-on");
  }
  Serial.printf("[boot] audio %s | camera %s\n", gAudioOk ? "OK" : "FAIL",
                gCamOk ? "OK" : "FAIL");
  Serial.printf("[boot] frame every %ums jpegQ=%d size_id=%d\n", FOLIO_FRAME_INTERVAL_MS,
                FOLIO_JPEG_QUALITY, FOLIO_FRAME_SIZE_ID);
  Serial.printf("[boot] audio %dHz chunk %dms\n", FOLIO_SAMPLE_RATE, FOLIO_CHUNK_MS);

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.printf("[boot] wifi ssid=%s\n", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  gWifiOk = WiFi.status() == WL_CONNECTED;
  if (gWifiOk) {
    Serial.printf("[boot] IP %s rssi=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
    pushEvent("boot", "{\"ok\":true}");
  } else {
    Serial.println("[boot] wifi offline — microSD captures; push when back");
  }

  nodeConfigBegin();
  gPushBackoffMaxMs = nodeConfig().pushBackoffMaxMs;

  gServer.on("/capture", HTTP_GET, handleCapture);
  gServer.on("/health", HTTP_GET, handleHealth);
  gServer.begin();

  gLastFrameMs = millis();
  gLastWifiTryMs = millis();
  Serial.println("[boot] capture -> microSD always, then push wifi+brain");
  Serial.println("[boot] =====================================");
}

void loop() {
  gServer.handleClient();
  ensureWifi();
  spoolTick();
  nodeConfigPoll();

  const NodeRuntimeConfig &cfg = nodeConfig();
  gPushBackoffMaxMs = cfg.pushBackoffMaxMs;

  const unsigned long now = millis();

  static unsigned long lastHeartbeat = 0;
  if (now - lastHeartbeat >= cfg.statusIntervalMs) {
    lastHeartbeat = now;
    Serial.printf(
        "[status] up=%lums wifi=%s cfg=%s frame=%lums spool a=%lu f=%lu push ok=%lu/%lu heap=%u\n",
        now - gBootMs, gWifiOk ? "up" : "down", cfg.version, cfg.frameIntervalMs,
        spoolOk() ? spoolPendingAudio() : 0, spoolOk() ? spoolPendingFrames() : 0,
        gAudioPushOk, gFramePushOk, ESP.getFreeHeap());
  }

  if (gAudioOk) {
    captureAudioChunk();
  }

  if (gCamOk && now - gLastFrameMs >= cfg.frameIntervalMs) {
    captureFrameChunk("interval");
  }

  drainSpoolOnce();

  delay(10);
}
