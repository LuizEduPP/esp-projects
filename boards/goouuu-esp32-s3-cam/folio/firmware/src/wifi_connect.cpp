#include "wifi_connect.h"

#include <WiFi.h>

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

static bool credsConfigured() {
  return WIFI_SSID[0] != '\0' && strcmp(WIFI_SSID, "SUA_REDE_WIFI") != 0;
}

static bool waitConnected(uint32_t timeoutMs) {
  const unsigned long deadline = millis() + timeoutMs;
  while (millis() < deadline && WiFi.status() != WL_CONNECTED) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

static bool connectOpen(uint32_t timeoutMs) {
  Serial.println("[wifi] scanning for open networks…");
  const int n = WiFi.scanNetworks(false, true);
  if (n <= 0) {
    Serial.println("[wifi] no APs found");
    return false;
  }

  String bestSsid;
  int bestRssi = -999;
  for (int i = 0; i < n; ++i) {
    const String ssid = WiFi.SSID(i);
    if (ssid.isEmpty()) {
      continue;
    }
    if (WiFi.encryptionType(i) != WIFI_AUTH_OPEN) {
      continue;
    }
    const int rssi = WiFi.RSSI(i);
    if (rssi > bestRssi) {
      bestRssi = rssi;
      bestSsid = ssid;
    }
  }
  WiFi.scanDelete();

  if (bestSsid.isEmpty()) {
    Serial.println("[wifi] no open network");
    return false;
  }

  Serial.printf("[wifi] open ssid=%s rssi=%d — connecting\n", bestSsid.c_str(), bestRssi);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(bestSsid.c_str());
  if (!waitConnected(timeoutMs)) {
    Serial.printf("[wifi] open connect failed ssid=%s\n", bestSsid.c_str());
    return false;
  }
  return true;
}

static bool connectConfigured(uint32_t timeoutMs) {
  if (!credsConfigured()) {
    return false;
  }
  Serial.printf("[wifi] configured ssid=%s — connecting\n", WIFI_SSID);
  WiFi.disconnect(true);
  delay(100);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  if (!waitConnected(timeoutMs)) {
    Serial.printf("[wifi] configured connect failed ssid=%s\n", WIFI_SSID);
    return false;
  }
  return true;
}

static bool folioWifiConnect(uint32_t timeoutMs) {
  if (connectOpen(timeoutMs)) {
    return true;
  }
  return connectConfigured(timeoutMs);
}

static void logConnected() {
  Serial.printf("[wifi] up ssid=%s ip=%s rssi=%d dBm\n", WiFi.SSID().c_str(),
                WiFi.localIP().toString().c_str(), WiFi.RSSI());
}

void folioWifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  Serial.println("[wifi] connecting…");
  if (folioWifiConnect(15000)) {
    logConnected();
  } else {
    Serial.println("[wifi] offline at boot — will rescan");
  }
}

void folioWifiMaintain(uint32_t retryMs) {
  static unsigned long lastTryMs = 0;
  static bool wasConnected = false;

  if (WiFi.status() == WL_CONNECTED) {
    if (!wasConnected) {
      wasConnected = true;
      logConnected();
    }
    return;
  }

  if (wasConnected) {
    wasConnected = false;
    Serial.println("[wifi] lost — rescanning…");
  }

  const unsigned long now = millis();
  if (now - lastTryMs < retryMs) {
    return;
  }
  lastTryMs = now;

  folioWifiConnect(retryMs);
}

bool folioWifiConnected() { return WiFi.status() == WL_CONNECTED; }

const char *folioWifiSsid() {
  if (WiFi.status() != WL_CONNECTED) {
    return "";
  }
  return WiFi.SSID().c_str();
}
