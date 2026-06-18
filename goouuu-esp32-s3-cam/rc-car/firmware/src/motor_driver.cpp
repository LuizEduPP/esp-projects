#include "motor_driver.h"
#include "pins.h"

#include "driver/gpio.h"

static constexpr uint8_t kDigital = 255;
static constexpr uint32_t kPwmHz = 25000;
static constexpr unsigned long kCmdTimeoutMs = 4000;

// {GPIO, LEDC} — ch1 reservado ao XCLK da câmera
static const struct {
  uint8_t gpio;
  uint8_t ch;
} kOut[] = {
    {PIN_L_IA1, 0}, {PIN_L_IB1, 2}, {PIN_L_IA2, 3}, {PIN_L_IB2, 4},
    {PIN_R_IA1, 5}, {PIN_R_IB1, kDigital}, {PIN_R_IA2, 6}, {PIN_R_IB2, 7},
};

static bool gReady = false;
static bool gRunning = false;
static int gLeft = 0;
static int gRight = 0;
static unsigned long gLastCmdMs = 0;

static void writeOut(int i, uint8_t duty) {
  const uint8_t gpio = kOut[i].gpio;
  if (kOut[i].ch == kDigital) {
    digitalWrite(gpio, duty > 127 ? HIGH : LOW);
    return;
  }
  if (gReady) {
    ledcWrite(kOut[i].ch, duty);
  }
}

static void drivePair(int a, int b, int speed) {
  speed = constrain(speed, -255, 255);
  const uint8_t duty = static_cast<uint8_t>(abs(speed));
  if (speed == 0) {
    writeOut(a, 0);
    writeOut(b, 0);
    return;
  }
  if (speed > 0) {
    writeOut(b, 0);
    writeOut(a, duty);
  } else {
    writeOut(a, 0);
    writeOut(b, duty);
  }
}

static void apply(int left, int right) {
  if (!gReady) {
    return;
  }
  drivePair(0, 1, left);
  drivePair(2, 3, left);
  drivePair(4, 5, right);
  drivePair(6, 7, right);
  gRunning = left != 0 || right != 0;
}

void motorBegin() {
  pinMode(PIN_R_IB1, OUTPUT);
  digitalWrite(PIN_R_IB1, LOW);
  for (const auto &o : kOut) {
    if (o.ch == kDigital) {
      continue;
    }
    ledcDetachPin(o.gpio);
    ledcSetup(o.ch, kPwmHz, 8);
    ledcAttachPin(o.gpio, o.ch);
    ledcWrite(o.ch, 0);
  }
  gReady = true;
  gRunning = false;
}

void motorSet(int left, int right) {
  if (!gReady) {
    motorBegin();
  }
  gLeft = constrain(left, -255, 255);
  gRight = constrain(right, -255, 255);
  gLastCmdMs = millis();
  apply(gLeft, gRight);
}

void motorPoll() {
  if (!gReady || gLastCmdMs == 0 || millis() - gLastCmdMs <= kCmdTimeoutMs) {
    return;
  }
  if (gRunning) {
    gLeft = 0;
    gRight = 0;
    apply(0, 0);
  }
}

void motorAfterCapture() {
  motorBegin();
  if (gLastCmdMs > 0 && millis() - gLastCmdMs <= kCmdTimeoutMs) {
    apply(gLeft, gRight);
  }
}
