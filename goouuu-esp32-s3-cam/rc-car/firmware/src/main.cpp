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

static WebServer server(80);
static portMUX_TYPE gCamMux = portMUX_INITIALIZER_UNLOCKED;
static int gLeft = 0;
static int gRight = 0;
static unsigned long gLastCmdMs = 0;
static constexpr unsigned long kCmdTimeoutMs = 4000;

static bool cameraBegin() {
  camera_config_t cfg = {};
  cfg.ledc_channel = LEDC_CHANNEL_1;
  cfg.ledc_timer = LEDC_TIMER_1;
  cfg.pin_d0 = CAM_PIN_D0;
  cfg.pin_d1 = CAM_PIN_D1;
  cfg.pin_d2 = CAM_PIN_D2;
  cfg.pin_d3 = CAM_PIN_D3;
  cfg.pin_d4 = CAM_PIN_D4;
  cfg.pin_d5 = CAM_PIN_D5;
  cfg.pin_d6 = CAM_PIN_D6;
  cfg.pin_d7 = CAM_PIN_D7;
  cfg.pin_xclk = CAM_PIN_XCLK;
  cfg.pin_pclk = CAM_PIN_PCLK;
  cfg.pin_vsync = CAM_PIN_VSYNC;
  cfg.pin_href = CAM_PIN_HREF;
  cfg.pin_sccb_sda = CAM_PIN_SIOD;
  cfg.pin_sccb_scl = CAM_PIN_SIOC;
  cfg.pin_pwdn = CAM_PIN_PWDN;
  cfg.pin_reset = CAM_PIN_RESET;
  cfg.xclk_freq_hz = 20000000;
  cfg.frame_size = FRAMESIZE_QVGA;
  cfg.pixel_format = PIXFORMAT_JPEG;
  cfg.grab_mode = CAMERA_GRAB_LATEST;
  cfg.fb_location = CAMERA_FB_IN_PSRAM;
  cfg.jpeg_quality = 14;
  cfg.fb_count = 2;

  return esp_camera_init(&cfg) == ESP_OK;
}

static camera_fb_t *captureFrame() {
  portENTER_CRITICAL(&gCamMux);
  camera_fb_t *fb = esp_camera_fb_get();
  portEXIT_CRITICAL(&gCamMux);
  return fb;
}

static void handleCapture() {
  camera_fb_t *fb = captureFrame();
  if (!fb) {
    server.send(503, "text/plain", "camera busy");
    return;
  }
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send_P(200, "image/jpeg", reinterpret_cast<const char *>(fb->buf), fb->len);
  esp_camera_fb_return(fb);
}

static int parseIntField(const String &body, const char *key) {
  const String token = String("\"") + key + "\":";
  const int pos = body.indexOf(token);
  if (pos < 0) {
    return 0;
  }
  return body.substring(pos + token.length()).toInt();
}

static void handleControl() {
  if (server.method() != HTTP_POST) {
    server.send(405, "text/plain", "POST only");
    return;
  }

  const String body = server.arg("plain");
  gLeft = constrain(parseIntField(body, "left"), -255, 255);
  gRight = constrain(parseIntField(body, "right"), -255, 255);
  gLastCmdMs = millis();
  motorDrive(gLeft, gRight);

  Serial.printf("control L=%d R=%d\n", gLeft, gRight);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

static void handleStatus() {
  String json = "{\"ip\":\"";
  json += WiFi.localIP().toString();
  json += "\",\"left\":";
  json += gLeft;
  json += ",\"right\":";
  json += gRight;
  json += "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

static void handleOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
  server.send(204);
}

static bool wifiBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("WiFi");
  for (int i = 0; i < 40; ++i) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(" OK");
      Serial.print("IP: ");
      Serial.println(WiFi.localIP());
      return true;
    }
    Serial.print(".");
    delay(500);
  }
  Serial.println(" FALHOU");
  return false;
}

static void serverBegin() {
  server.on("/capture", HTTP_GET, handleCapture);
  server.on("/control", HTTP_POST, handleControl);
  server.on("/control", HTTP_OPTIONS, handleOptions);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound([]() { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println("HTTP: GET /capture  POST /control  GET /status");
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("RC seguidor IA (LM Studio) — init");

  if (!cameraBegin()) {
    Serial.println("ERRO: camera");
    return;
  }
  Serial.println("Camera OK");

  if (!wifiBegin()) {
    return;
  }

  serverBegin();
  motorBegin();
  motorStop();
}

void loop() {
  server.handleClient();

  if (gLastCmdMs == 0 || millis() - gLastCmdMs > kCmdTimeoutMs) {
    motorStop();
    return;
  }

  motorDrive(gLeft, gRight);
}
