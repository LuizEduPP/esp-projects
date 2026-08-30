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
  int stage = 0;

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
      stage = 0;
    }
    last = now;
    return fell;
  }

  bool clicked() {
    const bool now = digitalRead(pin);
    bool fired = false;
    if (now == LOW && last == HIGH && millis() - lastChange > 200) {
      lastChange = millis();
      downAt = millis();
      stage = 0;
    } else if (now == HIGH && last == LOW) {
      fired = (stage == 0);
    }
    last = now;
    return fired;
  }

  bool heldFor(unsigned long ms, int level = 1) {
    if (last == LOW && stage < level && millis() - downAt >= ms) {
      stage = level;
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

static void tickClock() {
  dataUpdateClock(netTimeReady());
  dataUpdateSystem();
  uiRefreshValues();
}

static void syncUi(bool force);

static void refreshDash() {
  sNextFetch = millis() + DASH_API_INTERVAL_MS;
  uiBusy(true);
  uiSuspend();
  const bool ok = netFetchDash();
  uiResume();
  uiBusy(false);
  if (ok) uiRefreshValues();
}

static void refreshNow() {
  uiStatus("atualizando");
  refreshDash();
  syncUi(true);
  uiStatus("");
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
      refreshDash();
      uiShowDash();
      sNextOta = millis() + 20000;
      sNextTimeSync = millis() + DASH_TIME_SYNC_INTERVAL_MS;
    } else {
      uiShowDash();
    }
  }

  if (btnBoot.clicked()) {
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
  if (btnBoot.heldFor(DASH_HOLD_REFRESH_MS, 1)) {
    setNight(false);
    keepAwake();
    refreshNow();
  }
  if (btnBoot.heldFor(DASH_HOLD_FORGET_MS, 2)) {
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
    if (sNight && (long)(now - sNextFetch) >= 0) refreshDash();
    if (sNight && (long)(now - sNextUi) >= 0) syncUi(false);
    if (sNight && (long)(now - sNextOta) >= 0) {
      sNextOta = now + DASH_OTA_INTERVAL_MS;
      uiSuspend();
      otaCheck();
      uiResume();
    }
  }

  if (!sNight && (long)(now - sNightAt) >= 0) setNight(true);

  if ((long)(now - sNextTick) >= 0) {
    sNextTick = now + 1000;
    tickClock();
  }

  uiTask();
  delay(5);
}
