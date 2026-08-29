#include <Arduino.h>

#include <time.h>

#include "dash_config.h"
#include "net.h"
#include "pins.h"
#include "ui.h"

static bool sNight = false;
static unsigned long sNightAt = 0;

static Place sPlace;
static Weather sWeather;
static Air sAir;
static Market sMarket;
static DevStats sDev;

static bool sTimerRunning = false;
static bool sTimerBreak = false;
static int sTimerLeft = DASH_POMODORO_WORK_S;
static unsigned long sTimerTick = 0;
static char sInsight[DASH_AI_TEXT_MAX] = "";

static unsigned long sNextWeather = 0;
static unsigned long sNextInsight = 0;
static unsigned long sNextMarket = 0;
static unsigned long sNextDev = 0;
static unsigned long sNextTimeSync = 0;
static unsigned long sNextGeo = 0;
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
  if (ok) uiUpdateMoon();
}

static void refreshWeather() {
  sNextWeather = millis() + DASH_WEATHER_INTERVAL_MS;
  if (netFetchWeather(sPlace, sWeather)) uiUpdateWeather(sWeather);
  if (netFetchAir(sPlace, sAir)) uiUpdateAir(sAir, sWeather);
}

static void refreshInsight() {
  sNextInsight = millis() + DASH_AI_INTERVAL_MS;
  uiUpdateInsight(sInsight, true);
  uiTask();

  if (netFetchInsight(sPlace, sWeather, sInsight, sizeof(sInsight))) {
    Serial.printf("[ai] %s\n", sInsight);
  } else {
    snprintf(sInsight, sizeof(sInsight), "Ollama fora do ar.");
  }
  uiUpdateInsight(sInsight, false);
}

static void refreshMarket() {
  sNextMarket = millis() + DASH_MARKET_INTERVAL_MS;
  if (netFetchMarket(sMarket)) uiUpdateMarket(sMarket);
}

static void refreshDev() {
  sNextDev = millis() + DASH_DEV_INTERVAL_MS;
  netFetchDev(sDev);
  uiUpdateDev(sDev);
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

static void locate() {
  sNextGeo = millis() + 6UL * 3600000UL;
  if (netResolvePlace(sPlace)) {
    sWeather.valid = false;
    sNextWeather = millis();
  }
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
  sNextInsight = millis() + 5000;
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
      locate();
      sNextWeather = millis();
      sNextMarket = millis() + 2000;
      sNextDev = millis() + 4000;
      sNextTimeSync = millis() + DASH_TIME_SYNC_INTERVAL_MS;
    }
  }

  if (btnBoot.pressed()) {
    if (!sNight && uiPage() == PAGE_TIMER) {
      sTimerRunning = !sTimerRunning;
      sTimerTick = millis() + 1000;
      uiUpdateTimer(sTimerLeft, timerTotal(), sTimerBreak, sTimerRunning);
      keepAwake();
    } else {
      setNight(!sNight);
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
    if ((long)(now - sNextGeo) >= 0) locate();
    if ((long)(now - sNextWeather) >= 0) refreshWeather();
    if ((long)(now - sNextMarket) >= 0) refreshMarket();
    if ((long)(now - sNextDev) >= 0) refreshDev();
    if ((long)(now - sNextInsight) >= 0 && sWeather.valid) refreshInsight();
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
