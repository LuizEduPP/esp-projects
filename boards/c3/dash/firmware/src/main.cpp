#include <Arduino.h>

#include <time.h>

#include "dash_config.h"
#include "data.h"
#include "net.h"
#include "ota.h"
#include "pins.h"
#include "ui.h"
#include "uidoc.h"

static bool sNight = false;
static unsigned long sNightAt = 0;

static bool sTimerRunning = false;
static bool sTimerBreak = false;
static int sTimerLeft = DASH_POMODORO_WORK_S;
static unsigned long sTimerTick = 0;

static unsigned long sNextFetch = 0;
static unsigned long sNextUi = 0;
static unsigned long sNextOta = 0;
static unsigned long sNextTimeSync = 0;
static unsigned long sNextTick = 0;
static NetState sLastState = NET_OFFLINE;

struct Button {
  uint8_t pin;
  bool last = HIGH;
  unsigned long downAt = 0;
  unsigned long lastChange = 0;
  bool longFired = false;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    last = digitalRead(pin);
  }

  bool pressed() {
    const bool now = digitalRead(pin);
    const bool fell = (now == LOW && last == HIGH && millis() - lastChange > 200);
    if (fell) {
      lastChange = millis();
      downAt = millis();
      longFired = false;
    }
    last = now;
    return fell;
  }

  bool heldFor(unsigned long ms) {
    if (last == LOW && !longFired && millis() - downAt >= ms) {
      longFired = true;
      return true;
    }
    return false;
  }
};

static Button btnDown;
static Button btnUp;
static Button btnBoot;

static void keepAwake() { sNightAt = millis() + DASH_SCREEN_TIMEOUT_MS; }

static void setNight(bool on) {
  if (sNight == on) return;
  sNight = on;
  uiSetNight(on);
  if (!on) keepAwake();
}

static int timerTotal() {
  return sTimerBreak ? DASH_POMODORO_BREAK_S : DASH_POMODORO_WORK_S;
}

static void timerReset(bool breakMode) {
  sTimerBreak = breakMode;
  sTimerLeft = timerTotal();
  dataUpdateTimer(sTimerLeft, timerTotal(), sTimerBreak, sTimerRunning);
}

static void timerTask() {
  if (!sTimerRunning) return;
  const unsigned long now = millis();
  if ((long)(now - sTimerTick) < 0) return;
  sTimerTick = now + 1000;

  if (sTimerLeft > 0) --sTimerLeft;
  if (sTimerLeft == 0) {
    sTimerRunning = false;
    timerReset(!sTimerBreak);
    return;
  }
  dataUpdateTimer(sTimerLeft, timerTotal(), sTimerBreak, sTimerRunning);
}

static void tickClock() {
  dataUpdateClock(netTimeReady());
  dataUpdateSystem();
  if (!sNight) uiRefreshValues();
}

static void refreshDash() {
  sNextFetch = millis() + DASH_API_INTERVAL_MS;
  uiBusy(true);
  uiSuspend();
  const bool ok = netFetchDash();
  uiResume();
  uiBusy(false);
  if (ok) uiRefreshValues();
}

static void syncUi(bool force) {
  sNextUi = millis() + DASH_UI_POLL_MS;
  uiSuspend();
  const bool changed = uiDocSync(force);
  uiResume();
  if (changed) uiRebuild();
}

void setup() {
  Serial.begin(115200);
  btnDown.begin(BTN_DOWN);
  btnUp.begin(BTN_UP);
  btnBoot.begin(BTN_BOOT);

  uiBegin();
  uiSplash("dash", "iniciando");

  uiDocBegin();
  timerReset(false);
  dataUpdateClock(false);
  dataUpdateSystem();

  netBegin();
  sNextTimeSync = millis();
  keepAwake();
}

void loop() {
  netLoop();

  const NetState state = netState();
  if (state != sLastState) {
    sLastState = state;
    if (state == NET_PROVISIONING) {
      uiShowProvisioning(true);
    } else if (state == NET_ONLINE) {
      netSyncTime();
      syncUi(false);
      uiShowDash();
      sNextFetch = millis();
      sNextOta = millis() + 20000;
      sNextTimeSync = millis() + DASH_TIME_SYNC_INTERVAL_MS;
    } else {
      uiShowDash();
    }
  }

  if (btnBoot.pressed()) {
    if (sNight) {
      setNight(false);
    } else if (uiMenuOpen()) {
      uiMenuSelect();
      keepAwake();
    } else {
      uiMenuToggle();
      keepAwake();
    }
  }
  if (btnBoot.heldFor(3000)) {
    setNight(false);
    netForgetCredentials();
  }

  if (btnUp.pressed()) {
    if (sNight) {
      setNight(false);
    } else if (uiMenuOpen()) {
      uiMenuMove(-1);
      keepAwake();
    } else {
      uiTurnPage(1);
      keepAwake();
    }
  }

  if (btnDown.pressed()) {
    if (sNight) {
      setNight(false);
    } else if (uiMenuOpen()) {
      uiMenuMove(1);
      keepAwake();
    } else {
      uiTurnPage(-1);
      keepAwake();
    }
  }

  const unsigned long now = millis();
  if (netOnline()) {
    if (!netTimeReady() || (long)(now - sNextTimeSync) >= 0) {
      netSyncTime();
      sNextTimeSync = now + DASH_TIME_SYNC_INTERVAL_MS;
    }
    if ((long)(now - sNextFetch) >= 0) refreshDash();
    if ((long)(now - sNextUi) >= 0) syncUi(false);
    if ((long)(now - sNextOta) >= 0) {
      sNextOta = now + DASH_OTA_INTERVAL_MS;
      uiSuspend();
      otaCheck();
      uiResume();
    }
  }

  timerTask();

  if (!sNight && (long)(now - sNightAt) >= 0) setNight(true);

  if ((long)(now - sNextTick) >= 0) {
    sNextTick = now + 1000;
    tickClock();
  }

  uiTask();
  delay(5);
}
