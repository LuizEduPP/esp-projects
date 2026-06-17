#include "motor_driver.h"
#include "pins.h"

#include "driver/gpio.h"

static constexpr uint8_t kMotorPins[] = {
    PIN_L_IA1, PIN_L_IB1, PIN_L_IA2, PIN_L_IB2,
    PIN_R_IA1, PIN_R_IB1, PIN_R_IA2, PIN_R_IB2,
};

static constexpr uint32_t kPwmHz = 25000;
static constexpr uint8_t kPwmBits = 8;
static bool gReady = false;

static void disableRgbLed() {
  ledcDetachPin(PIN_RGB_LED);
  gpio_reset_pin(static_cast<gpio_num_t>(PIN_RGB_LED));
  pinMode(PIN_RGB_LED, OUTPUT);
  digitalWrite(PIN_RGB_LED, LOW);
}

static void writePin(uint8_t ch, uint8_t duty) {
  const uint8_t pin = kMotorPins[ch];
  if (pin == PIN_RGB_LED) {
    digitalWrite(PIN_RGB_LED, duty > 127 ? HIGH : LOW);
    return;
  }
  ledcWrite(ch, duty);
}

static void drivePair(uint8_t chA, uint8_t chB, int speed) {
  speed = constrain(speed, -255, 255);
  const uint8_t duty = static_cast<uint8_t>(abs(speed));

  if (speed == 0) {
    writePin(chA, 0);
    writePin(chB, 0);
    return;
  }
  if (speed > 0) {
    writePin(chB, 0);
    writePin(chA, duty);
  } else {
    writePin(chA, 0);
    writePin(chB, duty);
  }
}

void motorBegin() {
  if (gReady) {
    return;
  }
  disableRgbLed();
  for (uint8_t ch = 0; ch < sizeof(kMotorPins); ++ch) {
    const uint8_t pin = kMotorPins[ch];
    if (pin == PIN_RGB_LED) {
      continue;
    }
    gpio_reset_pin(static_cast<gpio_num_t>(pin));
    ledcSetup(ch, kPwmHz, kPwmBits);
    ledcAttachPin(pin, ch);
    ledcWrite(ch, 0);
  }
  gReady = true;
}

void motorDrive(int left, int right) {
  motorBegin();
  drivePair(0, 1, left);
  drivePair(2, 3, left);
  drivePair(4, 5, right);
  drivePair(6, 7, right);
}

void motorStop() {
  if (!gReady) {
    return;
  }
  drivePair(0, 1, 0);
  drivePair(2, 3, 0);
  drivePair(4, 5, 0);
  drivePair(6, 7, 0);
}
