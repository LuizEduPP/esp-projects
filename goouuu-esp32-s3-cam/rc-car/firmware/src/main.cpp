#include <Arduino.h>
#include <NimBLEDevice.h>

#include "motor_driver.h"
#include "pins.h"

// Nome curto — cabe no anuncio BLE (max 31 bytes)
static constexpr const char *kBleName = "GOOUUU-RC";

// Nordic UART (NUS) — CircuitMagic BLE Controller
static constexpr const char *kNusService = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr const char *kNusRx = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
static constexpr const char *kNusTx = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

static constexpr int kDefaultSpeed = 200;

static void driveTank(int left, int right) {
  if (left == 0 && right == 0) {
    motorStop();
  } else {
    motorDrive(constrain(left, -255, 255), constrain(right, -255, 255));
  }
}

static void driveJoystick(int x, int y) {
  if (abs(x) < 12 && abs(y) < 12) {
    motorStop();
    return;
  }
  const int fwd = map(y, -100, 100, -255, 255);
  const int turn = map(x, -100, 100, -255, 255);
  driveTank(constrain(fwd + turn, -255, 255), constrain(fwd - turn, -255, 255));
}

static void driveJoystick255(int x, int y) {
  driveJoystick((x - 128) * 100 / 128, (y - 128) * 100 / 128);
}

static bool parseTwoInts(const std::string &data, int &a, int &b) {
  return sscanf(data.c_str(), "%d,%d", &a, &b) == 2 ||
         sscanf(data.c_str(), "%d;%d", &a, &b) == 2 ||
         sscanf(data.c_str(), "%d %d", &a, &b) == 2 ||
         sscanf(data.c_str(), "%d:%d", &a, &b) == 2;
}

static bool applyLetter(char cmd) {
  const int s = kDefaultSpeed;
  switch (cmd) {
    case 'F':
    case 'U':
      driveTank(s, s);
      return true;
    case 'B':
    case 'D':
      driveTank(-s, -s);
      return true;
    case 'L':
      driveTank(-s, s);
      return true;
    case 'R':
      driveTank(s, -s);
      return true;
    case 'S':
    case 'X':
      motorStop();
      return true;
    case 'W':
      driveTank(s, s);
      return true;
    case 'A':
      driveTank(-s, s);
      return true;
    default:
      return false;
  }
}

static bool applyDigit(char cmd) {
  switch (cmd) {
    case '1':
    case '8':
      driveTank(kDefaultSpeed, kDefaultSpeed);
      return true;
    case '2':
    case '5':
      driveTank(-kDefaultSpeed, -kDefaultSpeed);
      return true;
    case '4':
      driveTank(-kDefaultSpeed, kDefaultSpeed);
      return true;
    case '6':
      driveTank(kDefaultSpeed, -kDefaultSpeed);
      return true;
    case '0':
    case '3':
    case '7':
    case '9':
      motorStop();
      return true;
    default:
      return false;
  }
}

static void handleBleDrive(const std::string &data) {
  if (data.empty()) {
    motorStop();
    return;
  }

  if (data.find("\xE2\x86\x91") != std::string::npos) {
    driveTank(kDefaultSpeed, kDefaultSpeed);
    return;
  }
  if (data.find("\xE2\x86\x93") != std::string::npos) {
    driveTank(-kDefaultSpeed, -kDefaultSpeed);
    return;
  }
  if (data.find("\xE2\x86\x90") != std::string::npos) {
    driveTank(-kDefaultSpeed, kDefaultSpeed);
    return;
  }
  if (data.find("\xE2\x86\x92") != std::string::npos) {
    driveTank(kDefaultSpeed, -kDefaultSpeed);
    return;
  }

  int a = 0;
  int b = 0;
  if (parseTwoInts(data, a, b)) {
    if (a == 0 && b == 0) {
      motorStop();
      return;
    }
    if (a >= 0 && a <= 255 && b >= 0 && b <= 255) {
      driveJoystick255(a, b);
      return;
    }
    if (abs(a) <= 100 && abs(b) <= 100) {
      driveJoystick(a, b);
      return;
    }
    driveTank(a, b);
    return;
  }

  bool handled = false;
  for (unsigned char raw : data) {
    if (raw == '\r' || raw == '\n' || raw == ' ' || raw == ',') {
      continue;
    }
    const char c = static_cast<char>(toupper(raw));
    if (c == '^' || c == 'I') {
      driveTank(kDefaultSpeed, kDefaultSpeed);
      handled = true;
      continue;
    }
    if (c == 'V' || c == 'J') {
      driveTank(-kDefaultSpeed, -kDefaultSpeed);
      handled = true;
      continue;
    }
    if (c == '<' || c == 'G') {
      driveTank(-kDefaultSpeed, kDefaultSpeed);
      handled = true;
      continue;
    }
    if (c == '>' || c == 'H') {
      driveTank(kDefaultSpeed, -kDefaultSpeed);
      handled = true;
      continue;
    }
    if (applyLetter(c) || applyDigit(c)) {
      handled = true;
    }
  }
  if (!handled) {
    motorStop();
  }
}

class BleServerCallbacks : public NimBLEServerCallbacks {
  void onDisconnect(NimBLEServer *server) override {
    motorStop();
    server->startAdvertising();
  }
};

class BleRxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *characteristic) override {
    handleBleDrive(characteristic->getValue());
  }
};

static void bleBegin() {
  NimBLEDevice::init(kBleName);
  NimBLEDevice::setPower(ESP_PWR_LVL_P9);

  NimBLEServer *server = NimBLEDevice::createServer();
  server->setCallbacks(new BleServerCallbacks());

  NimBLEService *service = server->createService(kNusService);
  NimBLECharacteristic *rx = service->createCharacteristic(
      kNusRx, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  rx->setCallbacks(new BleRxCallbacks());
  NimBLECharacteristic *tx = service->createCharacteristic(
      kNusTx, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  service->start();

  // So o nome no anuncio — UUID descoberto apos conectar
  NimBLEAdvertising *advertising = NimBLEDevice::getAdvertising();
  advertising->setName(kBleName);
  advertising->start();
}

void setup() {
  ledcDetachPin(PIN_RGB_LED);
  pinMode(PIN_RGB_LED, OUTPUT);
  digitalWrite(PIN_RGB_LED, LOW);
  bleBegin();
}

void loop() {
  if (NimBLEDevice::getServer()->getConnectedCount() == 0) {
    NimBLEDevice::startAdvertising();
  }
  delay(1000);
}
