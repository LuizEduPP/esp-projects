#include "motor_driver.h"
#include "pins.h"

#include "driver/gpio.h"

struct MotorOut {
  uint8_t gpio;
  uint8_t ledc;  // 255 = digital (GPIO48)
};

static const MotorOut kLeft[] = {
    {PIN_L_IA1, MOTOR_LEDC_L_IA1},
    {PIN_L_IB1, MOTOR_LEDC_L_IB1},
    {PIN_L_IA2, MOTOR_LEDC_L_IA2},
    {PIN_L_IB2, MOTOR_LEDC_L_IB2},
};

static const MotorOut kRight[] = {
    {PIN_R_IA1, MOTOR_LEDC_R_IA1},
    {PIN_R_IB1, MOTOR_LEDC_R_IB1},
    {PIN_R_IA2, MOTOR_LEDC_R_IA2},
    {PIN_R_IB2, MOTOR_LEDC_R_IB2},
};

static constexpr uint32_t kPwmHz = 25000;
static constexpr uint8_t kPwmBits = 8;
static bool gReady = false;

static void rgbLedAsGpio() {
  ledcDetachPin(PIN_RGB_LED);
  gpio_reset_pin(static_cast<gpio_num_t>(PIN_RGB_LED));
  pinMode(PIN_RGB_LED, OUTPUT);
  digitalWrite(PIN_RGB_LED, LOW);
}

static void writeOut(const MotorOut &out, uint8_t duty) {
  if (out.ledc == 255) {
    digitalWrite(out.gpio, duty > 127 ? HIGH : LOW);
    return;
  }
  ledcWrite(out.ledc, duty);
}

static void driveSide(const MotorOut *pair, int speed) {
  speed = constrain(speed, -255, 255);
  const uint8_t duty = static_cast<uint8_t>(abs(speed));

  if (speed == 0) {
    writeOut(pair[0], 0);
    writeOut(pair[1], 0);
    return;
  }
  if (speed > 0) {
    writeOut(pair[1], 0);
    writeOut(pair[0], duty);
  } else {
    writeOut(pair[0], 0);
    writeOut(pair[1], duty);
  }
}

static void setupPwmPin(const MotorOut &out) {
  if (out.ledc == 255) {
    return;
  }
  gpio_reset_pin(static_cast<gpio_num_t>(out.gpio));
  ledcSetup(out.ledc, kPwmHz, kPwmBits);
  ledcAttachPin(out.gpio, out.ledc);
  ledcWrite(out.ledc, 0);
}

void motorBegin() {
  if (gReady) {
    return;
  }
  rgbLedAsGpio();
  for (const auto &out : kLeft) {
    setupPwmPin(out);
  }
  for (const auto &out : kRight) {
    setupPwmPin(out);
  }
  gReady = true;
}

void motorDrive(int left, int right) {
  motorBegin();
  driveSide(kLeft, left);
  driveSide(kRight, right);
}

void motorStop() {
  if (!gReady) {
    return;
  }
  driveSide(kLeft, 0);
  driveSide(kRight, 0);
}
