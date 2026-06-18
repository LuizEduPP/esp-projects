#include "motor_driver.h"
#include "pins.h"

#include "driver/gpio.h"

static constexpr unsigned long kCmdTimeoutMs = 15000;
static constexpr unsigned long kWarnAtMs = kCmdTimeoutMs * 4 / 5;

static const char *kPinNames[] = {
    "L_IA1", "L_IB1", "L_IA2", "L_IB2",
    "R_IA1", "R_IB1", "R_IA2", "R_IB2",
};

static const uint8_t kGpio[] = {
    PIN_L_IA1, PIN_L_IB1, PIN_L_IA2, PIN_L_IB2,
    PIN_R_IA1, PIN_R_IB1, PIN_R_IA2, PIN_R_IB2,
};

static bool gReady = false;
static bool gRunning = false;
static bool gTimeoutWarned = false;
static int gLeft = 0;
static int gRight = 0;
static int gPrevLeft = 0;
static int gPrevRight = 0;
static unsigned long gLastCmdMs = 0;
static unsigned long gSetCount = 0;
static unsigned long gStopCount = 0;
static char gLastTag[32] = "boot";

static const char *dirLabel(int left, int right) {
  if (left == 0 && right == 0) {
    return "PARADO";
  }
  if (left > 0 && right > 0 && left == right) {
    return "FRENTE";
  }
  if (left < 0 && right < 0 && left == right) {
    return "TRAS";
  }
  if (left > 0 && right == 0) {
    return "GIRA_ESQ";
  }
  if (left == 0 && right > 0) {
    return "GIRA_DIR";
  }
  if (left > 0 && right > 0) {
    return "CURVA_FRENTE";
  }
  if (left < 0 && right < 0) {
    return "CURVA_TRAS";
  }
  return "MISTO";
}

static const char *sideMode(int speed) {
  if (speed == 0) {
    return "PARADO IA=0 IB=0";
  }
  if (speed > 0) {
    return "FRENTE IA=1 IB=0";
  }
  return "TRAS IA=0 IB=1";
}

static void pinOut(uint8_t gpio, bool on) {
  digitalWrite(gpio, on ? HIGH : LOW);
}

static void drivePair(uint8_t ia, uint8_t ib, int speed) {
  speed = constrain(speed, -255, 255);
  if (speed == 0) {
    pinOut(ia, false);
    pinOut(ib, false);
    return;
  }
  if (speed > 0) {
    pinOut(ib, false);
    pinOut(ia, true);
  } else {
    pinOut(ia, false);
    pinOut(ib, true);
  }
}

static void logPinStates() {
  Serial.print("[motor] GPIO:");
  for (uint8_t i = 0; i < 8; ++i) {
    Serial.printf(" %s(%d)=%d", kPinNames[i], kGpio[i], digitalRead(kGpio[i]));
  }
  Serial.println();
}

static void logApplyDetail(int left, int right) {
  Serial.printf("[motor] L9110S esq=%s | dir=%s\n", sideMode(left), sideMode(right));
  Serial.printf("[motor]   par1 L_IA1/IB1 GPIO%d/%d | par2 L_IA2/IB2 GPIO%d/%d\n",
                PIN_L_IA1, PIN_L_IB1, PIN_L_IA2, PIN_L_IB2);
  Serial.printf("[motor]   par1 R_IA1/IB1 GPIO%d/%d | par2 R_IA2/IB2 GPIO%d/%d\n",
                PIN_R_IA1, PIN_R_IB1, PIN_R_IA2, PIN_R_IB2);
  logPinStates();
  if (left != 0 || right != 0) {
    Serial.println("[motor] >>> ATIVO — se nao gira: VMOT/STBY/GND no L9110S <<<");
  }
}

static void apply(int left, int right) {
  if (!gReady) {
    Serial.println("[motor] ERRO apply sem init");
    return;
  }
  drivePair(kGpio[0], kGpio[1], left);
  drivePair(kGpio[2], kGpio[3], left);
  drivePair(kGpio[4], kGpio[5], right);
  drivePair(kGpio[6], kGpio[7], right);
  gRunning = left != 0 || right != 0;
}

void motorBegin() {
  Serial.println("[motor] init GPIO digital (sem PWM)");
  Serial.printf("[motor] timeout comando=%lums\n", kCmdTimeoutMs);
  for (uint8_t i = 0; i < 8; ++i) {
    const uint8_t pin = kGpio[i];
    gpio_reset_pin(static_cast<gpio_num_t>(pin));
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    Serial.printf("[motor]   %s GPIO%d -> OUT LOW\n", kPinNames[i], pin);
  }
  gReady = true;
  gRunning = false;
  gTimeoutWarned = false;
  strncpy(gLastTag, "init", sizeof(gLastTag));
}

void motorSet(int left, int right) {
  motorSetTagged(left, right, "set");
}

void motorSetTagged(int left, int right, const char *tag) {
  if (!gReady) {
    motorBegin();
  }
  if (tag) {
    strncpy(gLastTag, tag, sizeof(gLastTag) - 1);
    gLastTag[sizeof(gLastTag) - 1] = '\0';
  }

  const int prevL = gLeft;
  const int prevR = gRight;
  const unsigned long prevAge = gLastCmdMs ? millis() - gLastCmdMs : 0;

  gLeft = constrain(left, -255, 255);
  gRight = constrain(right, -255, 255);
  gLastCmdMs = millis();
  gSetCount++;
  gTimeoutWarned = false;

  if (gLeft == 0 && gRight == 0) {
    gStopCount++;
  }

  const bool changed = gLeft != gPrevLeft || gRight != gPrevRight;

  if (changed) {
    Serial.printf("[motor] MUDOU #%lu tag=%s | era L=%d R=%d (%s) ha %lums\n",
                  gSetCount, gLastTag, prevL, prevR, dirLabel(prevL, prevR), prevAge);
    Serial.printf("[motor] NOVO  L=%d R=%d (%s)\n", gLeft, gRight, dirLabel(gLeft, gRight));
    apply(gLeft, gRight);
    logApplyDetail(gLeft, gRight);
    gPrevLeft = gLeft;
    gPrevRight = gRight;
  } else {
    Serial.printf("[motor] REPETIR #%lu tag=%s L=%d R=%d (%s) ha %lums [keepalive]\n",
                  gSetCount, gLastTag, gLeft, gRight, dirLabel(gLeft, gRight), prevAge);
    apply(gLeft, gRight);
  }
}

void motorPoll() {
  if (!gReady || gLastCmdMs == 0) {
    return;
  }

  const unsigned long age = millis() - gLastCmdMs;

  if (gRunning && !gTimeoutWarned && age >= kWarnAtMs && age < kCmdTimeoutMs) {
    gTimeoutWarned = true;
    Serial.printf("[motor] AVISO sem novo /control ha %lums (timeout em %lums) — PC parou keepalive?\n",
                  age, kCmdTimeoutMs - age);
  }

  if (age <= kCmdTimeoutMs) {
    return;
  }

  if (gRunning) {
    Serial.printf("[motor] TIMEOUT %lums -> PARADO (ultimo L=%d R=%d tag=%s)\n",
                  age, gLeft, gRight, gLastTag);
    strncpy(gLastTag, "timeout", sizeof(gLastTag));
    gLeft = 0;
    gRight = 0;
    gPrevLeft = 0;
    gPrevRight = 0;
    apply(0, 0);
    logApplyDetail(0, 0);
  }
}

void motorLogHeartbeat() {
  static unsigned long lastBeat = 0;
  const unsigned long now = millis();
  if (now - lastBeat < 5000) {
    return;
  }
  lastBeat = now;

  const unsigned long age = gLastCmdMs ? now - gLastCmdMs : 0;
  Serial.printf("[status] uptime=%lus heap=%u motor=%s L=%d R=%d (%s) age=%lums sets=%lu tag=%s\n",
                now / 1000, ESP.getFreeHeap(), gRunning ? "ON" : "OFF", gLeft, gRight,
                dirLabel(gLeft, gRight), age, gSetCount, gLastTag);
}

String motorDiagJson() {
  String j = "{";
  j += "\"ready\":" + String(gReady ? "true" : "false");
  j += ",\"running\":" + String(gRunning ? "true" : "false");
  j += ",\"left\":" + String(gLeft);
  j += ",\"right\":" + String(gRight);
  j += ",\"dir\":\"" + String(dirLabel(gLeft, gRight)) + "\"";
  j += ",\"lastCmdMs\":" + String(gLastCmdMs);
  j += ",\"cmdAgeMs\":" + String(gLastCmdMs ? millis() - gLastCmdMs : 0);
  j += ",\"timeoutMs\":" + String(kCmdTimeoutMs);
  j += ",\"setCount\":" + String(gSetCount);
  j += ",\"stopCount\":" + String(gStopCount);
  j += ",\"lastTag\":\"" + String(gLastTag) + "\"";
  j += ",\"uptimeMs\":" + String(millis());
  j += ",\"pins\":[";
  for (uint8_t i = 0; i < 8; ++i) {
    if (i) {
      j += ",";
    }
    j += "{\"name\":\"" + String(kPinNames[i]) + "\"";
    j += ",\"gpio\":" + String(kGpio[i]);
    j += ",\"level\":" + String(digitalRead(kGpio[i]));
    j += ",\"expected\":\"";
    j += (gRunning ? "active" : "idle");
    j += "\"}";
  }
  j += "]}";
  return j;
}

void motorRunSelfTest() {
  struct Step {
    int l;
    int r;
    const char *tag;
    uint16_t ms;
  };
  static const Step kSteps[] = {
      {0, 0, "T0_stop", 800},
      {200, 200, "T1_frente", 2000},
      {0, 0, "T2_stop", 800},
      {200, 0, "T3_gira_esq", 1500},
      {0, 0, "T4_stop", 800},
      {0, 200, "T5_gira_dir", 1500},
      {0, 0, "T6_stop", 800},
      {-200, -200, "T7_tras", 1500},
      {0, 0, "T8_stop", 800},
  };

  Serial.println("[test] === bateria motores ===");
  for (const auto &s : kSteps) {
    Serial.printf("[test] --- %s L=%d R=%d (%ums) ---\n", s.tag, s.l, s.r, s.ms);
    motorSetTagged(s.l, s.r, s.tag);
    delay(s.ms);
  }
  strncpy(gLastTag, "test_done", sizeof(gLastTag));
  Serial.println("[test] === fim ===");
  Serial.println(motorDiagJson());
}
