#include "console_input.h"

#include "pins.h"

static ConsoleInput gCur;
static ConsoleInput gPrev;

static constexpr uint16_t DEBOUNCE_DIR_MS = 18;
static constexpr uint16_t DEBOUNCE_A_MS = 12;
static constexpr uint16_t DIR_REPEAT_DELAY_MS = 200;
static constexpr uint16_t DIR_REPEAT_RATE_MS = 65;

struct BtnDeb {
  bool raw = false;
  bool stable = false;
  uint32_t changedAt = 0;
};

static BtnDeb gDebA;
static BtnDeb gDebUp;
static BtnDeb gDebDown;
static BtnDeb gDebLeft;
static BtnDeb gDebRight;
static uint32_t gARepeatAt = 0;
static uint32_t gDirRepeatAt[4] = {};
static bool gALatch = false;

static bool readDebounced(uint8_t pin, BtnDeb &d, uint16_t ms) {
  const bool raw = digitalRead(pin) == LOW;
  const uint32_t now = millis();
  if (raw != d.raw) {
    d.raw = raw;
    d.changedAt = now;
  }
  if (now - d.changedAt >= ms) {
    d.stable = d.raw;
  }
  return d.stable;
}

static void updateRepeat(bool held, bool pressed, bool &repeat, uint32_t &nextAt,
                         uint16_t delayMs, uint16_t rateMs) {
  repeat = false;
  if (!held) {
    nextAt = 0;
    return;
  }
  const uint32_t now = millis();
  if (pressed) {
    repeat = true;
    nextAt = now + delayMs;
  } else if (nextAt != 0 && now >= nextAt) {
    repeat = true;
    nextAt = now + rateMs;
  }
}

bool consoleInputBegin() {
  pinMode(PIN_BTN_A, INPUT_PULLUP);
  pinMode(PIN_BTN_UP, INPUT_PULLUP);
  pinMode(PIN_BTN_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_LEFT, INPUT_PULLUP);
  pinMode(PIN_BTN_RIGHT, INPUT_PULLUP);
  delay(50);
  return true;
}

void consoleInputPoll() {
  gPrev = gCur;

  gCur.a = readDebounced(PIN_BTN_A, gDebA, DEBOUNCE_A_MS);
  gCur.up = readDebounced(PIN_BTN_UP, gDebUp, DEBOUNCE_DIR_MS);
  gCur.down = readDebounced(PIN_BTN_DOWN, gDebDown, DEBOUNCE_DIR_MS);
  gCur.left = readDebounced(PIN_BTN_LEFT, gDebLeft, DEBOUNCE_DIR_MS);
  gCur.right = readDebounced(PIN_BTN_RIGHT, gDebRight, DEBOUNCE_DIR_MS);

  gCur.leftPressed = gCur.left && !gPrev.left;
  gCur.rightPressed = gCur.right && !gPrev.right;
  gCur.upPressed = gCur.up && !gPrev.up;
  gCur.downPressed = gCur.down && !gPrev.down;
  gCur.aPressed = gCur.a && !gPrev.a;
  if (gCur.aPressed) {
    gALatch = true;
  }

  updateRepeat(gCur.up, gCur.upPressed, gCur.upRepeat, gDirRepeatAt[0],
               DIR_REPEAT_DELAY_MS, DIR_REPEAT_RATE_MS);
  updateRepeat(gCur.down, gCur.downPressed, gCur.downRepeat, gDirRepeatAt[1],
               DIR_REPEAT_DELAY_MS, DIR_REPEAT_RATE_MS);
  updateRepeat(gCur.left, gCur.leftPressed, gCur.leftRepeat, gDirRepeatAt[2],
               DIR_REPEAT_DELAY_MS, DIR_REPEAT_RATE_MS);
  updateRepeat(gCur.right, gCur.rightPressed, gCur.rightRepeat, gDirRepeatAt[3],
               DIR_REPEAT_DELAY_MS, DIR_REPEAT_RATE_MS);

  gCur.aRepeat = false;
  if (gCur.a) {
    if (gCur.aPressed) {
      gCur.aRepeat = true;
      gARepeatAt = millis() + 350;
    } else if (millis() >= gARepeatAt) {
      gCur.aRepeat = true;
      gARepeatAt = millis() + 120;
    }
  } else {
    gARepeatAt = 0;
  }
}

const ConsoleInput &consoleInputState() {
  return gCur;
}

bool consoleInputWantExit() {
  const ConsoleInput &in = consoleInputState();
  static uint32_t holdAt = 0;
  if (in.left && in.right) {
    if (holdAt == 0) {
      holdAt = millis();
    } else if (millis() - holdAt >= 1200) {
      holdAt = 0;
      return true;
    }
  } else {
    holdAt = 0;
  }
  return false;
}

bool consoleInputTakeA() {
  if (!gALatch) {
    return false;
  }
  gALatch = false;
  return true;
}
