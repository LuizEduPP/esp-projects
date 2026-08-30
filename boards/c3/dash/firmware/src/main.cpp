#include <Arduino.h>

#include <time.h>

#include "dash_config.h"
#include "net.h"
#include "pins.h"
#include "ui.h"

static bool sNight = false;
static unsigned long sNightAt = 0;

static Dash sDash;

static bool sTimerRunning = false;
static bool sTimerBreak = false;
static int sTimerLeft = DASH_POMODORO_WORK_S;
static unsigned long sTimerTick = 0;

static unsigned long sNextFetch = 0;
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

static void tickClock() {
  struct tm now;
  const bool ok = getLocalTime(&now, 20);
  if (!ok) memset(&now, 0, sizeof(now));
  uiUpdateClock(now, netTimeReady() && ok);
  uiUpdateSystem();
}

static void refreshDash() {
  sNextFetch = millis() + DASH_API_INTERVAL_MS;

  uiBusy(true);
  if (netFetchDash(sDash)) {
    if (sDash.tzOffsetSec != 0) netApplyTimezone(sDash.tzOffsetSec);
    uiUpdateDash(sDash);
  }
  uiBusy(false);
}

static int timerTotal() {
  return sTimerBreak ? DASH_POMODORO_BREAK_S : DASH_POMODORO_WORK_S;
}

static void timerReset(bool breakMode) {
  sTimerBreak = breakMode;
  sTimerLeft = timerTotal();
  uiUpdateTimer(sTimerLeft, timerTotal(), sTimerBreak, sTimerRunning);
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
  uiUpdateTimer(sTimerLeft, timerTotal(), sTimerBreak, sTimerRunning);
}

void setup() {
  Serial.begin(115200);
  btnDown.begin(BTN_DOWN);
  btnUp.begin(BTN_UP);
  btnBoot.begin(BTN_BOOT);

  uiBegin();
  uiSplash("dash", "iniciando");

  timerReset(false);

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
    } else {
      uiShowDash();
    }
    if (state == NET_ONLINE) {
      netSyncTime();
      sNextFetch = millis();
      sNextTimeSync = millis() + DASH_TIME_SYNC_INTERVAL_MS;
    }
  }

  if (btnBoot.pressed()) {
    if (sNight) {
      setNight(false);
    } else if (uiPage() == PAGE_TIMER) {
      sTimerRunning = !sTimerRunning;
      sTimerTick = millis() + 1000;
      uiUpdateTimer(sTimerLeft, timerTotal(), sTimerBreak, sTimerRunning);
      keepAwake();
    } else {
      if (netOnline()) refreshDash();
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
    } else {
      uiTurnPage(1);
      keepAwake();
    }
  }

  if (btnDown.pressed()) {
    if (sNight) {
      setNight(false);
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
