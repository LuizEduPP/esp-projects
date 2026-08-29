#include <Arduino.h>

#include <time.h>

#include "dash_config.h"
#include "net.h"
#include "pins.h"
#include "ui.h"

static int sPage = 0;
static bool sDirty = true;
static bool sNight = false;
static unsigned long sNightAt = 0;
static unsigned long sNextFrame = 0;

static Place sPlace;
static Weather sWeather;
static char sInsight[DASH_AI_TEXT_MAX] = "";
static bool sInsightPending = false;

static unsigned long sNextWeather = 0;
static unsigned long sNextInsight = 0;
static unsigned long sNextTimeSync = 0;
static unsigned long sNextGeo = 0;
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
  sDirty = true;
  if (!on) {
    uiIntro();
    keepAwake();
  }
}

static void refreshWeather() {
  sNextWeather = millis() + DASH_WEATHER_INTERVAL_MS;
  if (netFetchWeather(sPlace, sWeather)) sDirty = true;
}

static void refreshInsight() {
  sNextInsight = millis() + DASH_AI_INTERVAL_MS;
  sInsightPending = true;
  if (!sNight && sPage == 2) uiInsight(sInsight, true, sPage);

  if (netFetchInsight(sPlace, sWeather, sInsight, sizeof(sInsight))) {
    Serial.printf("[ai] %s\n", sInsight);
  } else {
    snprintf(sInsight, sizeof(sInsight), "Ollama fora do ar.");
  }
  sInsightPending = false;
  sDirty = true;
}

static void locate() {
  sNextGeo = millis() + 6UL * 3600000UL;
  if (netResolvePlace(sPlace)) {
    sWeather.valid = false;
    sNextWeather = millis();
    sDirty = true;
  }
}

static void render() {
  if (sNight) {
    struct tm now;
    const bool ok = getLocalTime(&now, 50);
    if (!ok) memset(&now, 0, sizeof(now));
    uiNight(now, netTimeReady() && ok);
    sDirty = false;
    return;
  }

  if (netState() == NET_PROVISIONING) {
    uiProvisioning(true);
    sDirty = false;
    return;
  }

  switch (sPage) {
    case 0: {
      struct tm now;
      const bool ok = getLocalTime(&now, 50);
      if (!ok) memset(&now, 0, sizeof(now));
      uiClock(now, netTimeReady() && ok, sPage);
      break;
    }
    case 1:
      uiWeather(sPlace, sWeather, sPage);
      break;
    case 2:
      uiInsight(sInsight, sInsightPending, sPage);
      break;
    default:
      uiSystem(sPlace, sPage);
      break;
  }
  sDirty = false;
}

void setup() {
  Serial.begin(115200);
  btnDown.begin(BTN_DOWN);
  btnUp.begin(BTN_UP);
  btnBoot.begin(BTN_BOOT);

  uiBegin();
  uiSplash("DASH", "iniciando");

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
    sDirty = true;
    if (state == NET_ONLINE) {
      netSyncTime();
      locate();
      sNextWeather = millis();
      sNextTimeSync = millis() + DASH_TIME_SYNC_INTERVAL_MS;
    }
  }

  if (btnBoot.pressed()) setNight(!sNight);
  if (btnBoot.heldFor(3000)) {
    setNight(false);
    netForgetCredentials();
    sDirty = true;
  }

  if (btnDown.pressed()) {
    if (sNight) {
      setNight(false);
    } else {
      sPage = (sPage + 1) % UI_PAGES;
      uiIntro();
      sDirty = true;
      keepAwake();
    }
  }

  if (btnUp.pressed()) {
    if (sNight) {
      setNight(false);
    } else {
      sPage = (sPage + UI_PAGES - 1) % UI_PAGES;
      uiIntro();
      sDirty = true;
      keepAwake();
    }
  }

  const unsigned long now = millis();
  if (netOnline()) {
    if (!netTimeReady() || (long)(now - sNextTimeSync) >= 0) {
      netSyncTime();
      sNextTimeSync = now + DASH_TIME_SYNC_INTERVAL_MS;
      sDirty = true;
    }
    if ((long)(now - sNextGeo) >= 0) locate();
    if ((long)(now - sNextWeather) >= 0) refreshWeather();
    if ((long)(now - sNextInsight) >= 0 && sWeather.valid) refreshInsight();
  }

  if (!sNight && (long)(now - sNightAt) >= 0) setNight(true);

  if ((long)(now - sNextFrame) >= 0) {
    sNextFrame = now + (sNight ? 1000 : 100);
    sDirty = true;
  }

  if (sDirty) render();
  delay(5);
}
