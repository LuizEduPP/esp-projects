#include "wifi_health.h"

#include <HTTPClient.h>
#include <WiFi.h>
#include <cstdarg>
#include <cstring>

#include "firmware_version.h"
#include "wifi_ota.h"
#include "wifi_store.h"

static const char *gDeviceId = "";
static const char *gRole = "";
static bool gLog = false;
static bool gEnabled = false;
static bool gBootFetchDone = false;
static uint32_t gLastHealthMs = 0;
static uint32_t gReconnectMs = 0;
static uint32_t gReconnectDelay = 1000;
static const uint32_t HEALTH_INTERVAL_MS = 30000;

static String gSsid;
static String gPass;
static String gServerUrl;

static void logMsg(const char *fmt, ...) {
  if (!gLog) {
    return;
  }
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}

static void loadCredentials() {
  wifiStoreLoad(gSsid, gPass, gServerUrl);
  gEnabled = gSsid.length() > 0;
}

static bool ensureWifi() {
  if (!gEnabled) {
    return false;
  }
  if (WiFi.status() == WL_CONNECTED) {
    gReconnectDelay = 1000;
    return true;
  }

  const uint32_t now = millis();
  if (now - gReconnectMs < gReconnectDelay) {
    return false;
  }
  gReconnectMs = now;

  logMsg("[wifi] conectando %s...", gSsid.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.begin(gSsid.c_str(), gPass.c_str());

  if (gReconnectDelay < 30000) {
    gReconnectDelay *= 2;
    if (gReconnectDelay > 30000) {
      gReconnectDelay = 30000;
    }
  }
  return false;
}

static bool httpGet(const char *path) {
  HTTPClient http;
  const String url = gServerUrl + path;
  if (!http.begin(url)) {
    logMsg("[wifi] GET %s falhou (begin)", path);
    return false;
  }
  const int code = http.GET();
  const String body = http.getString();
  http.end();
  logMsg("[wifi] GET %s -> %d %s", path, code, body.c_str());
  return code >= 200 && code < 300;
}

static bool httpPostHealth(bool pairReady, uint32_t uptimeSec, String &responseBody) {
  HTTPClient http;
  const String url = gServerUrl + "/api/v1/health";
  if (!http.begin(url)) {
    logMsg("[wifi] POST health falhou (begin)");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const String payload = String("{\"device_id\":\"") + gDeviceId +
                         "\",\"role\":\"" + gRole +
                         "\",\"uptime_sec\":" + uptimeSec +
                         ",\"pair_ready\":" + (pairReady ? "true" : "false") +
                         ",\"firmware_version\":\"" FW_VERSION "\"}";
  const int code = http.POST(payload);
  responseBody = http.getString();
  http.end();
  logMsg("[wifi] POST health -> %d %s", code, responseBody.c_str());
  return code >= 200 && code < 300;
}

static void fetchBootResource() {
  if (gBootFetchDone || gServerUrl.length() == 0) {
    return;
  }
  gBootFetchDone = true;

  if (strcmp(gRole, "s3") == 0) {
    const String path = String("/api/v1/config?device_id=") + gDeviceId;
    httpGet(path.c_str());
  } else if (strcmp(gRole, "cyd") == 0) {
    const String path = String("/api/v1/ui/state?device_id=") + gDeviceId;
    httpGet(path.c_str());
  }
}

void wifiHealthBegin(const char *deviceId, const char *role, bool logToSerial) {
  gDeviceId = deviceId;
  gRole = role;
  gLog = logToSerial;
  loadCredentials();

  if (!gEnabled) {
    logMsg("[wifi] aguardando credenciais (NVS ou CONFIG da CYD)");
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(gSsid.c_str(), gPass.c_str());
  logMsg("[wifi] init role=%s ssid=%s server=%s fw=%s", gRole, gSsid.c_str(), gServerUrl.c_str(), FW_VERSION);
}

void wifiHealthReload(bool logToSerial) {
  gLog = logToSerial;
  gBootFetchDone = false;
  WiFi.disconnect(true);
  delay(100);
  loadCredentials();
  if (gEnabled) {
    WiFi.begin(gSsid.c_str(), gPass.c_str());
    logMsg("[wifi] recarregado ssid=%s", gSsid.c_str());
  }
}

void wifiHealthLoop(bool pairReady, uint32_t uptimeSec, bool otaAllowed) {
  if (!gEnabled) {
    return;
  }

  if (!ensureWifi()) {
    return;
  }

  const uint32_t now = millis();
  if (now - gLastHealthMs < HEALTH_INTERVAL_MS) {
    return;
  }
  gLastHealthMs = now;

  String healthBody;
  if (httpPostHealth(pairReady, uptimeSec, healthBody)) {
    fetchBootResource();
    if (otaAllowed) {
      wifiOtaApplyFromHealthJson(healthBody, gLog);
    }
  }
}

bool wifiHealthConnected() {
  return gEnabled && WiFi.status() == WL_CONNECTED;
}

bool wifiHealthEnabled() {
  return gEnabled;
}
