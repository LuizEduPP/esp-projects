#include <Arduino.h>
#include <WiFi.h>
#include "camera_capture.h"
#include "pins.h"
#include "wifi_camera_upload.h"
#include "wifi_health.h"
#include "wifi_store.h"

static HardwareSerial PeerSerial(1);

static String peerLine;
static bool pairOk = false;
static bool pairReady = false;
static bool streamEnabled = false;
static bool capturing = false;
static uint32_t lastHelloMs = 0;
static uint32_t lastAutoCaptureMs = 0;
static uint32_t pairOkMs = 0;
static uint32_t lastStatusMs = 0;
static uint32_t bootMs = 0;
static int nextFrameId = 0;

static const uint32_t AUTO_CAPTURE_MS = STREAM_INTERVAL_MS;

static String pendingSsid;
static String pendingPass;
static String pendingServer;

static void printBanner() {
  Serial.println();
  Serial.println("=== edge-s3 uart-peer ===");
  Serial.println("[diag] JPEG bruto -> server | UART: pareamento + CONFIG");
  Serial.flush();
}

static void printStatus() {
  Serial.printf("[diag] uart=%s wifi=%s stream=%s cam=%s\n",
                pairReady ? "ok" : "...",
                wifiHealthConnected() ? "ok" : "off",
                streamEnabled ? "on" : "off",
                cameraCaptureReady() ? "ok" : "off");
  Serial.flush();
}

static void notifyPeerFrame(int frameId, bool ok) {
  if (!pairReady) {
    return;
  }
  PeerSerial.printf("FRAME:%d,%s\n", frameId, ok ? "ok" : "err");
  PeerSerial.flush();
}

static void captureAndUpload() {
  if (capturing) {
    return;
  }
  capturing = true;

  const uint8_t *buf = nullptr;
  size_t len = 0;
  if (!cameraCaptureRaw(&buf, &len)) {
    capturing = false;
    return;
  }

  const int frameId = ++nextFrameId;
  const bool uploaded = wifiCameraUploadFrame(buf, len, frameId, true);
  cameraCaptureRelease();
  notifyPeerFrame(frameId, uploaded);

  capturing = false;
}

static void applyWifiConfig() {
  if (pendingSsid.length() == 0) {
    return;
  }
  wifiStoreSave(pendingSsid, pendingPass, pendingServer);
  wifiHealthReload(true);
  Serial.printf("[wifi] CONFIG aplicado ssid=%s\n", pendingSsid.c_str());
  pendingSsid = "";
  pendingPass = "";
  pendingServer = "";
}

static void handleConfigLine(const String &line) {
  const int eq = line.indexOf('=');
  if (eq < 0) {
    return;
  }
  const String key = line.substring(8, eq);
  const String val = line.substring(eq + 1);

  if (key == "wifi_ssid") {
    pendingSsid = val;
  } else if (key == "wifi_pass") {
    pendingPass = val;
  } else if (key == "server_url") {
    pendingServer = val;
    applyWifiConfig();
  }
}

static void handlePeerLine(const String &line) {
  if (line.startsWith("PAIR:OK")) {
    if (!pairOk) {
      pairOk = true;
      pairOkMs = millis();
      Serial.printf("[peer] << %s\n", line.c_str());
      PeerSerial.println("PAIR:READY");
      Serial.println("[peer] >> PAIR:READY");
      PeerSerial.flush();
    }
    return;
  }

  if (line.startsWith("PAIR:READY")) {
    if (!pairReady) {
      pairReady = true;
      Serial.println("[peer] link pronto");
    }
    return;
  }

  if (line.startsWith("CONFIG:")) {
    handleConfigLine(line);
    return;
  }

  if (line.startsWith("STREAM:ON")) {
    streamEnabled = true;
    Serial.println("[peer] << STREAM:ON");
    lastAutoCaptureMs = 0;
    return;
  }

  if (line.startsWith("STREAM:OFF")) {
    streamEnabled = false;
    Serial.println("[peer] << STREAM:OFF");
    return;
  }

  if (line.startsWith("CAPTURE")) {
    if (pairReady) {
      Serial.println("[peer] << CAPTURE");
      captureAndUpload();
    }
    return;
  }

  if (line.startsWith("ets ") || line.indexOf("rst:0x") >= 0) {
    Serial.printf("[diag] CYD boot: %s\n", line.c_str());
  }
}

static void pollPeer() {
  while (PeerSerial.available()) {
    const char c = static_cast<char>(PeerSerial.read());
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

void setup() {
  Serial.begin(115200);
  delay(500);
  bootMs = millis();

  cameraCaptureBegin();
  wifiHealthBegin("s3-cam-01", "s3", true);
  PeerSerial.setRxBufferSize(2048);
  PeerSerial.begin(PEER_UART_BAUD, SERIAL_8N1, PIN_PEER_RX, PIN_PEER_TX);
  printBanner();
}

void loop() {
  pollPeer();

  const uint32_t now = millis();
  const uint32_t uptimeSec = (now - bootMs) / 1000;

  wifiHealthLoop(pairReady, uptimeSec);

  if (pairOk && !pairReady && now - pairOkMs >= 5000) {
    pairOk = false;
    pairOkMs = 0;
    Serial.println("[peer] handshake timeout, retry HELLO");
  }

  // Stream só com par UART ativo (CYD manda STREAM:ON) — nunca standalone.
  if (streamEnabled && pairReady && wifiHealthConnected() && !capturing &&
      now - lastAutoCaptureMs >= AUTO_CAPTURE_MS) {
    lastAutoCaptureMs = now;
    captureAndUpload();
  }

  if (!pairOk && now - lastHelloMs >= 2000) {
    lastHelloMs = now;
    PeerSerial.println("PAIR:HELLO,ID=s3-cam-01");
    Serial.println("[peer] >> PAIR:HELLO");
  }

  if (pairReady && now - lastHelloMs >= 10000) {
    lastHelloMs = now;
    PeerSerial.println("HEARTBEAT");
  }

  if (!pairReady && now - lastStatusMs >= 5000) {
    lastStatusMs = now;
    printStatus();
  }

  delay(10);
}
