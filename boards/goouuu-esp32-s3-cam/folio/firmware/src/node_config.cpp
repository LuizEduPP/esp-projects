#include "node_config.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <esp_camera.h>

#include "folio_config.h"

#ifndef FOLIO_BRAIN_URL
#define FOLIO_BRAIN_URL "http://192.168.1.28:8770"
#endif
#ifndef FOLIO_DEVICE_ID
#define FOLIO_DEVICE_ID "folio-s3-01"
#endif

static NodeRuntimeConfig gCfg = {};
static char gVersionHeader[24] = "";
static unsigned long gLastFetchMs = 0;

static long jsonNestedLong(const String &body, const char *section, const char *key,
                           long fallback) {
  const String sec = String("\"") + section + "\":{";
  int si = body.indexOf(sec);
  if (si < 0) {
    return fallback;
  }
  const String needle = String("\"") + key + "\":";
  int i = body.indexOf(needle, si);
  if (i < 0) {
    return fallback;
  }
  i += needle.length();
  return body.substring(i).toInt();
}

static String jsonString(const String &body, const char *key) {
  const String needle = String("\"") + key + "\":\"";
  int i = body.indexOf(needle);
  if (i < 0) {
    return "";
  }
  i += needle.length();
  int j = body.indexOf('"', i);
  if (j < 0) {
    return "";
  }
  return body.substring(i, j);
}

static void applyDefaults() {
  snprintf(gCfg.version, sizeof(gCfg.version), "boot");
  gCfg.frameIntervalMs = FOLIO_FRAME_INTERVAL_MS;
  gCfg.jpegQuality = FOLIO_JPEG_QUALITY;
  gCfg.frameSizeId = FOLIO_FRAME_SIZE_ID;
  gCfg.audioChunkMs = FOLIO_CHUNK_MS;
  gCfg.audioSampleRate = FOLIO_SAMPLE_RATE;
  gCfg.wifiRetryMs = FOLIO_WIFI_RETRY_MS;
  gCfg.pushBackoffMaxMs = FOLIO_PUSH_BACKOFF_MAX_MS;
  gCfg.statusIntervalMs = FOLIO_STATUS_INTERVAL_MS;
  snprintf(gVersionHeader, sizeof(gVersionHeader), "boot");
}

static void applyCameraQuality(uint8_t q) {
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_quality(s, q);
  }
}

static bool fetchFromBrain() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  String url = String(FOLIO_BRAIN_URL) + "/api/node/config";
  if (!http.begin(url)) {
    return false;
  }
  http.setTimeout(8000);
  http.addHeader("X-Folio-Device-Id", FOLIO_DEVICE_ID);
  const int code = http.GET();
  if (code != 200) {
    Serial.printf("[config] fetch HTTP %d\n", code);
    http.end();
    return false;
  }

  const String body = http.getString();
  http.end();

  const String version = jsonString(body, "version");
  if (version.length() > 0) {
    version.toCharArray(gCfg.version, sizeof(gCfg.version));
    snprintf(gVersionHeader, sizeof(gVersionHeader), "%s", gCfg.version.c_str());
  }

  gCfg.frameIntervalMs = (uint32_t)jsonNestedLong(body, "frames", "captureIntervalMs",
                                                  gCfg.frameIntervalMs);
  gCfg.jpegQuality =
      (uint8_t)jsonNestedLong(body, "frames", "jpegQuality", gCfg.jpegQuality);
  gCfg.frameSizeId = (uint8_t)jsonNestedLong(body, "frames", "sizeId", gCfg.frameSizeId);
  gCfg.audioChunkMs =
      (uint16_t)jsonNestedLong(body, "audio", "chunkMs", gCfg.audioChunkMs);
  gCfg.audioSampleRate =
      (uint16_t)jsonNestedLong(body, "audio", "sampleRate", gCfg.audioSampleRate);
  gCfg.wifiRetryMs =
      (uint32_t)jsonNestedLong(body, "node", "wifiRetryMs", gCfg.wifiRetryMs);
  gCfg.pushBackoffMaxMs = (uint32_t)jsonNestedLong(body, "node", "pushBackoffMaxMs",
                                                     gCfg.pushBackoffMaxMs);
  gCfg.statusIntervalMs = (uint32_t)jsonNestedLong(body, "node", "statusIntervalMs",
                                                   gCfg.statusIntervalMs);

  if (gCfg.jpegQuality != FOLIO_JPEG_QUALITY) {
    applyCameraQuality(gCfg.jpegQuality);
  }

  if (gCfg.frameSizeId != FOLIO_FRAME_SIZE_ID) {
    Serial.printf("[config] size_id=%u needs reflash (compiled %d)\n", gCfg.frameSizeId,
                  FOLIO_FRAME_SIZE_ID);
  }
  if (gCfg.audioChunkMs != FOLIO_CHUNK_MS || gCfg.audioSampleRate != FOLIO_SAMPLE_RATE) {
    Serial.printf("[config] audio %uHz/%ums — reflash for buffer match (compiled %d/%d)\n",
                  gCfg.audioSampleRate, gCfg.audioChunkMs, FOLIO_SAMPLE_RATE,
                  FOLIO_CHUNK_MS);
  }

  Serial.printf(
      "[config] synced v=%s frame=%lums jpegQ=%u wifi=%lums status=%lums\n",
      gCfg.version, gCfg.frameIntervalMs, gCfg.jpegQuality, gCfg.wifiRetryMs,
      gCfg.statusIntervalMs);
  return true;
}

void nodeConfigBegin() {
  applyDefaults();
  fetchFromBrain();
  gLastFetchMs = millis();
}

bool nodeConfigPoll() {
  const unsigned long now = millis();
  if (now - gLastFetchMs < gCfg.statusIntervalMs) {
    return false;
  }
  gLastFetchMs = now;
  return fetchFromBrain();
}

const NodeRuntimeConfig &nodeConfig() { return gCfg; }

const char *nodeConfigVersionHeader() { return gVersionHeader; }
