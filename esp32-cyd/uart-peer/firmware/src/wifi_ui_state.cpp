#include "wifi_ui_state.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "ui_layout.h"
#include "wifi_store.h"

static const char *gDeviceId = "cyd-01";
static const uint32_t FETCH_INTERVAL_MS = 15000;
static const int MAX_LINES = 4;

static String gTitle = "Edge Pair";
static String gLines[MAX_LINES];
static int gLineCount = 0;
static String gAlert;
static uint32_t gLastFetchMs = 0;
static bool gNeedsRedraw = false;

static String gServerUrl;

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

static void extractJsonLines(const String &json) {
  gLineCount = 0;
  const int idx = json.indexOf("\"lines\"");
  if (idx < 0) {
    return;
  }
  const int arrStart = json.indexOf('[', idx);
  const int arrEnd = json.indexOf(']', arrStart);
  if (arrStart < 0 || arrEnd < 0) {
    return;
  }

  const String arr = json.substring(arrStart + 1, arrEnd);
  int pos = 0;
  while (gLineCount < MAX_LINES && pos < static_cast<int>(arr.length())) {
    const int q1 = arr.indexOf('"', pos);
    if (q1 < 0) {
      break;
    }
    const int q2 = arr.indexOf('"', q1 + 1);
    if (q2 < 0) {
      break;
    }
    gLines[gLineCount++] = arr.substring(q1 + 1, q2);
    pos = q2 + 1;
  }
}

static void parseUiStateJson(const String &json) {
  const String title = extractJsonString(json, "title");
  if (title.length() > 0) {
    gTitle = title;
  }

  extractJsonLines(json);

  gAlert = "";
  const int alertIdx = json.indexOf("\"alert\"");
  if (alertIdx >= 0) {
    const int nullIdx = json.indexOf("null", alertIdx);
    const int strIdx = json.indexOf('"', json.indexOf(':', alertIdx) + 1);
    if (strIdx >= 0 && (nullIdx < 0 || strIdx < nullIdx)) {
      const int valStart = strIdx + 1;
      const int valEnd = json.indexOf('"', valStart);
      if (valEnd > valStart) {
        gAlert = json.substring(valStart, valEnd);
      }
    }
  }
}

static bool fetchUiState() {
  String ssid;
  String pass;
  wifiStoreLoad(ssid, pass, gServerUrl);
  if (gServerUrl.length() == 0 || WiFi.status() != WL_CONNECTED) {
    return false;
  }

  HTTPClient http;
  const String url = gServerUrl + "/api/v1/ui/state?device_id=" + gDeviceId;
  if (!http.begin(url)) {
    return false;
  }
  const int code = http.GET();
  const String body = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    return false;
  }
  parseUiStateJson(body);
  return true;
}

void wifiUiStateBegin(const char *deviceId) {
  gDeviceId = deviceId;
  gLines[0] = "UART peer ativo";
  gLines[1] = "Aguardando eventos...";
  gLineCount = 2;
}

void wifiUiStateLoop(bool wifiConnected) {
  if (!wifiConnected) {
    return;
  }

  const uint32_t now = millis();
  if (gLastFetchMs != 0 && now - gLastFetchMs < FETCH_INTERVAL_MS) {
    return;
  }
  gLastFetchMs = now;
  if (fetchUiState()) {
    gNeedsRedraw = true;
  }
}

bool wifiUiStateTakeRedraw() {
  if (!gNeedsRedraw) {
    return false;
  }
  gNeedsRedraw = false;
  return true;
}

void wifiUiStateDrawOverlay(LGFX_CYD &tft) {
  const int contentY = uiContentY(tft);
  const int contentH = uiContentH(tft);
  const int boxW = tft.width() - 16;
  const int boxH = 28;
  const int boxX = 8;
  const int boxY = contentY + contentH - boxH - 4;

  if (boxY < contentY) {
    return;
  }

  tft.fillRect(boxX, boxY, boxW, boxH, tft.color888(8, 8, 24));
  tft.drawRect(boxX, boxY, boxW, boxH, TFT_DARKGREY);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(boxX + 4, boxY + 4);
  tft.print(gTitle.c_str());

  if (gLineCount > 0) {
    tft.setTextColor(TFT_LIGHTGREY);
    tft.setCursor(boxX + 4, boxY + 15);
    String line = gLines[0];
    if (gLineCount > 1) {
      line += " | ";
      line += gLines[1];
    }
    if (line.length() > 42) {
      line = line.substring(0, 42);
    }
    tft.print(line.c_str());
  }

  if (gAlert.length() > 0) {
    tft.setTextColor(TFT_RED);
    tft.setCursor(boxX + boxW - tft.textWidth(gAlert.c_str()) - 4, boxY + 15);
    tft.print(gAlert.c_str());
  }
}
