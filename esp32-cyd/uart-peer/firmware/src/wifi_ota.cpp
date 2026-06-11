#include "wifi_ota.h"

#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <cstdarg>

static WifiOtaUiFn gUi = nullptr;

static void uiMsg(const char *msg) {
  if (gUi) {
    gUi(msg);
  }
}

static void logOta(bool log, const char *fmt, ...) {
  if (!log) {
    return;
  }
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.println(buf);
}

void wifiOtaSetUiCallback(WifiOtaUiFn fn) {
  gUi = fn;
}

static String extractJsonString(const String &json, const char *key) {
  const String needle = String("\"") + key + "\":\"";
  const int start = json.indexOf(needle);
  if (start < 0) {
    return "";
  }
  const int valStart = start + needle.length();
  const int valEnd = json.indexOf('"', valStart);
  if (valEnd < 0) {
    return "";
  }
  return json.substring(valStart, valEnd);
}

static bool hasFirmwareUpdate(const String &json) {
  const int idx = json.indexOf("\"firmware_update\"");
  if (idx < 0) {
    return false;
  }
  const int trueIdx = json.indexOf("\"update\":true", idx);
  const int trueSpIdx = json.indexOf("\"update\": true", idx);
  return trueIdx >= 0 || trueSpIdx >= 0;
}

bool wifiOtaApplyUrl(const char *url, const char *version, bool logToSerial) {
  if (url == nullptr || url[0] == '\0') {
    return false;
  }

  logOta(logToSerial, "[ota] baixando %s -> %s", version ? version : "?", url);
  uiMsg("OTA: baixando...");

  WiFiClient client;
  HTTPUpdate httpUpdate;
  httpUpdate.rebootOnUpdate(true);

  const t_httpUpdate_return ret = httpUpdate.update(client, url);
  switch (ret) {
  case HTTP_UPDATE_FAILED:
    logOta(logToSerial, "[ota] falhou: %s", httpUpdate.getLastErrorString().c_str());
    uiMsg("OTA: falhou");
    return false;
  case HTTP_UPDATE_NO_UPDATES:
    logOta(logToSerial, "[ota] sem atualizacao");
    return false;
  case HTTP_UPDATE_OK:
    logOta(logToSerial, "[ota] ok, reiniciando");
    uiMsg("OTA: ok");
    return true;
  default:
    return false;
  }
}

bool wifiOtaApplyFromHealthJson(const String &healthBody, bool logToSerial) {
  if (!hasFirmwareUpdate(healthBody)) {
    return false;
  }

  const int blockStart = healthBody.indexOf("\"firmware_update\"");
  if (blockStart < 0) {
    return false;
  }
  const int blockEnd = healthBody.indexOf('}', blockStart);
  const String block = (blockEnd > blockStart)
                           ? healthBody.substring(blockStart, blockEnd + 1)
                           : healthBody.substring(blockStart);

  const String url = extractJsonString(block, "url");
  const String version = extractJsonString(block, "version");
  if (url.length() == 0) {
    return false;
  }

  return wifiOtaApplyUrl(url.c_str(), version.c_str(), logToSerial);
}
