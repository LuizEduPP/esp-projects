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
static bool gCamOk = false;
static uint32_t gCaptureCount = 0;
static uint32_t gControlCount = 0;
static unsigned long gBootMs = 0;

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
  c.frame_size = FRAMESIZE_QVGA;
  c.pixel_format = PIXFORMAT_JPEG;
  c.grab_mode = CAMERA_GRAB_LATEST;
  c.fb_location = CAMERA_FB_IN_PSRAM;
  c.jpeg_quality = 12;
  c.fb_count = 2;
  return esp_camera_init(&c) == ESP_OK;
}

static void handleCapture() {
  logReq("/capture");
  gCaptureCount++;
  portENTER_CRITICAL(&gCamMux);
  camera_fb_t *fb = esp_camera_fb_get();
  portEXIT_CRITICAL(&gCamMux);
  if (!fb) {
    Serial.println("[http] capture FALHOU");
    gServer.send(503, "text/plain", "busy");
    return;
  }
  Serial.printf("[http] capture OK %ux%u %u bytes\n", fb->width, fb->height, fb->len);
  cors();
  gServer.send_P(200, "image/jpeg", reinterpret_cast<const char *>(fb->buf), fb->len);
  esp_camera_fb_return(fb);
}

static int jsonInt(const String &body, const char *key) {
  const String tok = String("\"") + key + "\":";
  const int pos = body.indexOf(tok);
  return pos < 0 ? 0 : body.substring(pos + tok.length()).toInt();
}

static bool jsonBool(const String &body, const char *key) {
  const String tok = String("\"") + key + "\":";
  const int pos = body.indexOf(tok);
  if (pos < 0) {
    return false;
  }
  const String rest = body.substring(pos + tok.length());
  return rest.startsWith("true");
}

static void handleControl() {
  logReq("/control");
  gControlCount++;
  if (gServer.method() != HTTP_POST) {
    gServer.send(405, "text/plain", "POST");
    return;
  }
  const String &body = gServer.arg("plain");
  Serial.printf("[http] body: %s\n", body.c_str());
  const int l = jsonInt(body, "left");
  const int r = jsonInt(body, "right");
  const bool hold = jsonBool(body, "hold");
  if (body.indexOf("\"left\"") < 0 || body.indexOf("\"right\"") < 0) {
    Serial.println("[http] AVISO JSON sem left/right — usando 0");
  }
  Serial.printf("[http] parsed L=%d R=%d hold=%s (control #%lu)\n", l, r,
                hold ? "true" : "false", gControlCount);
  motorSetHold(hold);
  motorSetTagged(l, r, hold ? "hold" : "http");
  cors();
  String resp = "{\"ok\":true,\"left\":";
  resp += l;
  resp += ",\"right\":";
  resp += r;
  resp += ",\"motor\":";
  resp += motorDiagJson();
  resp += "}";
  Serial.printf("[http] /control -> running=%s\n",
                (l != 0 || r != 0) ? "SIM" : "NAO");
  gServer.send(200, "application/json", resp);
}

static void handleDiag() {
  logReq("/diag");
  cors();
  String j = "{\"ip\":\"";
  j += WiFi.localIP().toString();
  j += "\",\"camera\":";
  j += gCamOk ? "true" : "false";
  j += ",\"captures\":";
  j += gCaptureCount;
  j += ",\"controls\":";
  j += gControlCount;
  j += ",\"heap\":";
  j += ESP.getFreeHeap();
  j += ",\"motor\":";
  j += motorDiagJson();
  j += "}";
  gServer.send(200, "application/json", j);
}

static void handleTest() {
  logReq("/test");
  Serial.println("[http] /test — bateria no ESP (bloqueia ~12s)");
  motorRunSelfTest();
  cors();
  String j = "{\"ok\":true,\"motor\":";
  j += motorDiagJson();
  j += "}";
  gServer.send(200, "application/json", j);
}

static void handleStatus() {
  logReq("/status");
  cors();
  String j = "{\"ip\":\"";
  j += WiFi.localIP().toString();
  j += "\",\"motor\":";
  j += motorDiagJson();
  j += "}";
  gServer.send(200, "application/json", j);
}

void setup() {
  Serial.begin(115200);
  delay(300);
  gBootMs = millis();
  Serial.println("\n[boot] ========== RC car diag ==========");
  Serial.printf("[boot] heap=%u psram=%u\n", ESP.getFreeHeap(), ESP.getFreePsram());

  motorBegin();

  gCamOk = cameraBegin();
  Serial.printf("[boot] camera %s\n", gCamOk ? "OK" : "FALHOU");
  if (!gCamOk) {
    Serial.println("[boot] motores funcionam sem camera");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (int i = 0; i < 40 && WiFi.status() != WL_CONNECTED; ++i) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[boot] ERRO wifi — motores via serial apenas");
    return;
  }
  Serial.printf("[boot] IP %s rssi=%d dBm\n", WiFi.localIP().toString().c_str(), WiFi.RSSI());
  Serial.println("[boot] endpoints: GET /capture /diag /status | POST /control /test");
  Serial.println("[boot] logs: [http]=rede [motor]=motores [status]=heartbeat 5s");
  Serial.println("[boot] =====================================");

  gServer.on("/capture", HTTP_GET, handleCapture);
  gServer.on("/control", HTTP_POST, handleControl);
  gServer.on("/diag", HTTP_GET, handleDiag);
  gServer.on("/test", HTTP_POST, handleTest);
  gServer.on("/status", HTTP_GET, handleStatus);
  gServer.begin();
}

void loop() {
  gServer.handleClient();
  motorPoll();
  motorLogHeartbeat();
}
