#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_camera.h>

#include "motor_driver.h"
#include "pins.h"

#ifndef WIFI_SSID
#define WIFI_SSID "SUA_REDE_WIFI"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "SUA_SENHA_WIFI"
#endif

static WebServer gServer(80);
static portMUX_TYPE gCamMux = portMUX_INITIALIZER_UNLOCKED;

static void cors() {
  gServer.sendHeader("Access-Control-Allow-Origin", "*");
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

static void handleCapture() {
  portENTER_CRITICAL(&gCamMux);
  camera_fb_t *fb = esp_camera_fb_get();
  portEXIT_CRITICAL(&gCamMux);
  if (!fb) {
    gServer.send(503, "text/plain", "busy");
    return;
  }
  cors();
  gServer.send_P(200, "image/jpeg", reinterpret_cast<const char *>(fb->buf), fb->len);
  esp_camera_fb_return(fb);
  motorAfterCapture();
}

static int jsonInt(const String &body, const char *key) {
  const String tok = String("\"") + key + "\":";
  const int pos = body.indexOf(tok);
  return pos < 0 ? 0 : body.substring(pos + tok.length()).toInt();
}

static void handleControl() {
  if (gServer.method() != HTTP_POST) {
    gServer.send(405, "text/plain", "POST");
    return;
  }
  const String &body = gServer.arg("plain");
  motorSet(jsonInt(body, "left"), jsonInt(body, "right"));
  cors();
  gServer.send(200, "application/json", "{\"ok\":true}");
}

static void handleStatus() {
  cors();
  gServer.send(200, "application/json",
                "{\"ip\":\"" + WiFi.localIP().toString() + "\"}");
}

void setup() {
  Serial.begin(115200);
  if (!cameraBegin()) {
    Serial.println("ERRO camera");
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(500);
  }
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("ERRO wifi");
    return;
  }
  Serial.println(WiFi.localIP());

  gServer.on("/capture", HTTP_GET, handleCapture);
  gServer.on("/control", HTTP_POST, handleControl);
  gServer.on("/status", HTTP_GET, handleStatus);
  gServer.begin();
  motorBegin();  // depois da camera (LEDC ch1 = XCLK)
}

void loop() {
  gServer.handleClient();
  motorPoll();
}
