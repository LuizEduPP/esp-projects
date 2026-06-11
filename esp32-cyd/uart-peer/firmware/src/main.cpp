#include <Arduino.h>
#include <WiFi.h>
#include "display_cyd.hpp"
#include "jpeg_preview.h"
#include "pins.h"
#include "touch_input.h"
#include "wifi_camera_fetch.h"
#include "wifi_health.h"
#include "wifi_ota.h"
#include "wifi_store.h"
#include "wifi_ui_state.h"
#include "ui_layout.h"

#define LED_RED   4
#define LED_GREEN 16
#define LED_BLUE  17
#define BTN_BOOT  0

static LGFX_CYD tft;
static String peerLine;
static bool linkReady = false;
static bool configSent = false;
static uint32_t bootMs = 0;
static uint32_t lastBtnMs = 0;
static bool btnWasDown = false;
static bool liveStreamEnabled = true;

static String wifiSsid;
static String wifiPass;
static String wifiServer;

static void ledsOff() {
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

static void blinkLed(int pin, int times, int ms = 120) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, LOW);
    delay(ms);
    digitalWrite(pin, HIGH);
    delay(ms);
  }
}

static void peerWriteLine(const char *line) {
  Serial.println(line);
  Serial.flush();
}

static void onOtaUi(const char *msg) {
  if (msg == nullptr) {
    return;
  }
  const int y = uiFooterY(tft);
  tft.fillRect(0, y, tft.width(), UI_FOOTER_H, TFT_NAVY);
  tft.setCursor(8, y + 6);
  tft.setTextColor(TFT_WHITE);
  tft.print(msg);
}

static void drawMainStatus() {
  char line1[48];
  char line2[48];
  snprintf(line1, sizeof(line1), "CYD | UART %s", linkReady ? "OK" : "...");
  if (wifiHealthConnected()) {
    snprintf(line2, sizeof(line2), "WiFi: %s", wifiSsid.c_str());
  } else if (wifiSsid.length() > 0) {
    snprintf(line2, sizeof(line2), "WiFi: conectando...");
  } else {
    snprintf(line2, sizeof(line2), "WiFi: secrets.h / NVS");
  }
  jpegPreviewDrawStatus(tft, line1, line2);
}

static void drawContentArea() {
  const int y = uiContentY(tft);
  const int h = uiContentH(tft);
  tft.fillRect(0, y, tft.width(), h, TFT_BLACK);
  tft.setCursor(8, y + 16);
  tft.setTextColor(TFT_LIGHTGREY);
  tft.print("Aguardando stream...");
}

static void sendWifiConfigToS3() {
  if (!linkReady || configSent || !wifiStoreHasCredentials()) {
    return;
  }
  wifiStoreLoad(wifiSsid, wifiPass, wifiServer);
  Serial.printf("CONFIG:wifi_ssid=%s\n", wifiSsid.c_str());
  Serial.printf("CONFIG:wifi_pass=%s\n", wifiPass.c_str());
  Serial.printf("CONFIG:server_url=%s\n", wifiServer.c_str());
  Serial.flush();
  configSent = true;
  blinkLed(LED_BLUE, 1, 80);
}

static void sendStreamOn() {
  if (!linkReady) {
    return;
  }
  peerWriteLine("STREAM:ON");
}

static void requestCapture() {
  if (!linkReady) {
    return;
  }
  blinkLed(LED_GREEN, 1, 60);
  peerWriteLine("CAPTURE");
  wifiCameraFetchKick();
}

static void handlePeerLine(const String &line) {
  if (line.startsWith("PAIR:HELLO")) {
    const bool wasReady = linkReady;
    linkReady = false;
    configSent = false;
    blinkLed(LED_GREEN, 2, 80);
    peerWriteLine("PAIR:OK,ID=cyd-01");
    if (wasReady) {
      drawMainStatus();
    }
  } else if (line.startsWith("PAIR:READY")) {
    blinkLed(LED_GREEN, 1, 100);
    peerWriteLine("PAIR:READY");
    if (!linkReady) {
      linkReady = true;
      drawMainStatus();
      drawContentArea();
      sendWifiConfigToS3();
      sendStreamOn();
      wifiCameraFetchKick();
    }
  } else if (line.startsWith("HEARTBEAT")) {
    peerWriteLine("ACK:HEARTBEAT");
  } else if (line.startsWith("FRAME:")) {
    if (line.indexOf(",ok") >= 0) {
      blinkLed(LED_BLUE, 1, 30);
      wifiCameraFetchKick();
    }
  }
}

static void pollPeer() {
  while (Serial.available()) {
    const char c = static_cast<char>(Serial.read());
    if (c == '\n') {
      handlePeerLine(peerLine);
      peerLine = "";
    } else if (c != '\r') {
      peerLine += c;
      if (peerLine.length() > 255) {
        peerLine = "";
      }
    }
  }
}

static void pollCaptureButton() {
  const bool down = digitalRead(BTN_BOOT) == LOW;
  const uint32_t now = millis();
  if (down && !btnWasDown && now - lastBtnMs > 400) {
    lastBtnMs = now;
    requestCapture();
  }
  btnWasDown = down;
}

static void initWifi() {
  wifiStoreLoad(wifiSsid, wifiPass, wifiServer);
  wifiUiStateBegin("cyd-01");
  if (wifiSsid.length() > 0) {
    wifiHealthBegin("cyd-01", "cyd", false);
  }
}

void setup() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(BTN_BOOT, INPUT_PULLUP);
  ledsOff();

  bootMs = millis();

  displaySetup(tft);
  jpegPreviewBegin(tft);
  wifiCameraFetchBegin(tft);

  initWifi();
  displayEnsureLandscape(tft);
  wifiOtaSetUiCallback(onOtaUi);

  Serial.setRxBufferSize(2048);
  Serial.begin(PEER_UART_BAUD, SERIAL_8N1, PIN_PEER_RX, PIN_PEER_TX);
  delay(300);

  drawMainStatus();
  drawContentArea();
  blinkLed(LED_GREEN, 3);
}

void loop() {
  pollPeer();
  pollCaptureButton();

  if (wifiHealthConnected() && linkReady) {
    wifiCameraFetchLoop(true, liveStreamEnabled);
    wifiUiStateLoop(true);
    if (wifiUiStateTakeRedraw() && !liveStreamEnabled) {
      drawMainStatus();
      drawContentArea();
      wifiUiStateDrawOverlay(tft);
    }
  }

  const uint32_t uptimeSec = (millis() - bootMs) / 1000;
  wifiHealthLoop(linkReady, uptimeSec, true);

  delay(2);
}
