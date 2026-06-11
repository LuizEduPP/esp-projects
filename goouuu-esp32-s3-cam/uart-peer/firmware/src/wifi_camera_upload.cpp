#include "wifi_camera_upload.h"

#include <HTTPClient.h>
#include <WiFi.h>

#include "wifi_store.h"

bool wifiCameraUploadFrame(const uint8_t *data, size_t len, int frameId, bool logToSerial) {
  if (data == nullptr || len < 512) {
    return false;
  }
  if (WiFi.status() != WL_CONNECTED) {
    if (logToSerial) {
      Serial.println("[cam] upload skip: wifi off");
    }
    return false;
  }

  String ssid;
  String pass;
  String serverUrl;
  wifiStoreLoad(ssid, pass, serverUrl);
  if (serverUrl.length() == 0) {
    if (logToSerial) {
      Serial.println("[cam] upload skip: sem server_url");
    }
    return false;
  }

  HTTPClient http;
  const String url = serverUrl + "/api/v1/camera/frame?device_id=s3-cam-01&frame_id=" + frameId;
  if (!http.begin(url)) {
    if (logToSerial) {
      Serial.println("[cam] upload begin falhou");
    }
    return false;
  }

  http.addHeader("Content-Type", "image/jpeg");
  http.setTimeout(15000);
  const int code = http.POST(const_cast<uint8_t *>(data), len);
  const String body = http.getString();
  http.end();

  if (logToSerial) {
    Serial.printf("[cam] upload -> %d %s (%u B)\n", code, body.c_str(), static_cast<unsigned>(len));
  }
  return code >= 200 && code < 300;
}
