#include "wifi_camera_fetch.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "jpeg_preview.h"
#include "wifi_store.h"

static LGFX_CYD *gTft = nullptr;
static uint32_t gLastBackupMs = 0;
static uint32_t gLastFrameId = 0;
static bool gFetching = false;
static bool gUartFetchPending = false;

// UART (FRAME:ok) dispara fetch; poll HTTP só como rede de segurança.
static const uint32_t BACKUP_POLL_MS = 3000;
static const size_t DISPLAY_MAX = 32768;

static bool parseInfoJson(const String &json, uint32_t &frameId, bool &changed) {
  frameId = 0;
  changed = false;

  const int key = json.indexOf("\"frame_id\"");
  if (key < 0) {
    return false;
  }
  const int colon = json.indexOf(':', key);
  const int comma = json.indexOf(',', colon);
  if (colon < 0 || comma < 0) {
    return false;
  }
  frameId = static_cast<uint32_t>(json.substring(colon + 1, comma).toInt());

  const int changedKey = json.indexOf("\"changed\"");
  if (changedKey >= 0) {
    changed = json.indexOf("true", changedKey) >= 0;
  } else {
    changed = frameId > gLastFrameId;
  }
  return true;
}

static bool pollFrameInfo(uint32_t &frameId, bool &changed) {
  String ssid;
  String pass;
  String serverUrl;
  wifiStoreLoad(ssid, pass, serverUrl);
  if (serverUrl.length() == 0) {
    return false;
  }

  HTTPClient http;
  const String url = serverUrl + "/api/v1/camera/frame/info?device_id=s3-cam-01&since=" +
                     String(gLastFrameId);
  if (!http.begin(url)) {
    return false;
  }
  http.setTimeout(3000);
  const int code = http.GET();
  const String body = http.getString();
  http.end();
  if (code < 200 || code >= 300) {
    return false;
  }
  return parseInfoJson(body, frameId, changed);
}

static uint32_t readFrameIdHeader(HTTPClient &http, uint32_t fallback) {
  if (http.hasHeader("X-Frame-Id")) {
    return static_cast<uint32_t>(http.header("X-Frame-Id").toInt());
  }
  if (http.hasHeader("x-frame-id")) {
    return static_cast<uint32_t>(http.header("x-frame-id").toInt());
  }
  return fallback;
}

static bool fetchDisplayFrame() {
  if (!gTft) {
    return false;
  }

  String ssid;
  String pass;
  String serverUrl;
  wifiStoreLoad(ssid, pass, serverUrl);
  if (serverUrl.length() == 0) {
    return false;
  }

  HTTPClient http;
  const String url = serverUrl + "/api/v1/camera/frame/display?device_id=s3-cam-01&since=" +
                     String(gLastFrameId);
  if (!http.begin(url)) {
    return false;
  }
  http.setTimeout(10000);
  const int code = http.GET();
  if (code != 200) {
    http.end();
    return false;
  }

  const uint32_t headerFrameId = readFrameIdHeader(http, gLastFrameId + 1);
  int len = http.getSize();
  if (len <= 0) {
    len = static_cast<int>(DISPLAY_MAX);
  }
  if (len > static_cast<int>(DISPLAY_MAX)) {
    http.end();
    gLastFrameId = headerFrameId;
    return false;
  }

  static uint8_t buf[DISPLAY_MAX];
  WiFiClient *stream = http.getStreamPtr();
  size_t received = 0;
  const uint32_t started = millis();
  while (http.connected() && (len <= 0 || received < static_cast<size_t>(len))) {
    const size_t avail = stream->available();
    if (avail > 0) {
      const size_t toRead = min(avail, DISPLAY_MAX - received);
      received += stream->readBytes(buf + received, toRead);
    } else if (!http.connected() && !stream->available()) {
      break;
    } else if (millis() - started > 8000) {
      break;
    } else {
      delay(1);
    }
  }
  http.end();

  if (received < 100 || buf[0] != 0xFF || buf[1] != 0xD8) {
    return false;
  }
  if (!jpegPreviewShow(*gTft, buf, received)) {
    return false;
  }
  gLastFrameId = headerFrameId;
  return true;
}

static void runFetch() {
  if (gFetching || !gTft) {
    return;
  }
  gFetching = true;
  fetchDisplayFrame();
  gFetching = false;
}

void wifiCameraFetchBegin(LGFX_CYD &tft) {
  gTft = &tft;
  gLastFrameId = 0;
  gLastBackupMs = 0;
  gUartFetchPending = true;
}

void wifiCameraFetchKick() {
  gUartFetchPending = true;
}

void wifiCameraFetchLoop(bool wifiConnected, bool streamEnabled) {
  if (!wifiConnected || !streamEnabled || gFetching || !gTft) {
    return;
  }

  if (gUartFetchPending) {
    gUartFetchPending = false;
    runFetch();
    return;
  }

  const uint32_t now = millis();
  if (gLastBackupMs != 0 && now - gLastBackupMs < BACKUP_POLL_MS) {
    return;
  }
  gLastBackupMs = now;

  uint32_t frameId = 0;
  bool changed = false;
  if (!pollFrameInfo(frameId, changed)) {
    return;
  }
  if (!changed || frameId == 0 || frameId <= gLastFrameId) {
    return;
  }

  runFetch();
}
