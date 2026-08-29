#include <Arduino.h>

#include <time.h>

#include "dash_config.h"
#include "net.h"
#include "pins.h"
#include "ui.h"

enum Screen { SCREEN_MENU, SCREEN_CLOCK, SCREEN_WEATHER, SCREEN_AI };

static const char *const kMenu[] = {"Hora & data", "Clima", "AI"};
static const int kMenuCount = sizeof(kMenu) / sizeof(kMenu[0]);

static Screen sScreen = SCREEN_MENU;
static int sMenuIndex = 0;
static bool sDirty = true;

static Weather sWeather;
static char sInsight[DASH_AI_TEXT_MAX] = "";
static bool sInsightPending = false;

static unsigned long sNextWeather = 0;
static unsigned long sNextInsight = 0;
static unsigned long sNextTimeSync = 0;
static int sLastSecond = -1;

struct Button {
  uint8_t pin;
  bool last = HIGH;
  unsigned long lastChange = 0;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);
    last = digitalRead(pin);
  }

  bool pressed() {
    const bool now = digitalRead(pin);
    const bool fell = (now == LOW && last == HIGH && millis() - lastChange > 200);
    if (fell) lastChange = millis();
    last = now;
    return fell;
  }
};

static Button btnNav;
static Button btnSel;

static void refreshWeather() {
  sNextWeather = millis() + DASH_WEATHER_INTERVAL_MS;
  if (netFetchWeather(sWeather)) sDirty = true;
}

static void refreshInsight() {
  sNextInsight = millis() + DASH_AI_INTERVAL_MS;
  sInsightPending = true;
  if (sScreen == SCREEN_AI) uiInsight(sInsight, true, netOnline());

  if (!netFetchInsight(sWeather, sInsight, sizeof(sInsight))) {
    snprintf(sInsight, sizeof(sInsight), "Ollama fora do ar.");
  }
  sInsightPending = false;
  sDirty = true;
}

static void render() {
  const bool online = netOnline();
  switch (sScreen) {
    case SCREEN_MENU:
      uiMenu(kMenu, kMenuCount, sMenuIndex, online);
      break;
    case SCREEN_CLOCK: {
      struct tm now;
      const bool ok = getLocalTime(&now, 50);
      if (!ok) memset(&now, 0, sizeof(now));
      uiClock(now, netTimeReady() && ok, online);
      break;
    }
    case SCREEN_WEATHER:
      uiWeather(sWeather, online);
      break;
    case SCREEN_AI:
      uiInsight(sInsight, sInsightPending, online);
      break;
  }
  sDirty = false;
}

void setup() {
  Serial.begin(115200);
  btnNav.begin(BTN_NAV);
  btnSel.begin(BTN_SEL);

  uiBegin();
  uiSplash("DASH", "conectando...");

  netBegin();
  for (int i = 0; i < 40 && !netOnline(); ++i) delay(250);

  if (netOnline()) {
    netSyncTime();
    refreshWeather();
  }
  sNextTimeSync = millis() + 3600000UL;
  sNextInsight = millis();
}

void loop() {
  netEnsure();

  if (btnNav.pressed()) {
    switch (sScreen) {
      case SCREEN_MENU:
        sMenuIndex = (sMenuIndex + 1) % kMenuCount;
        break;
      case SCREEN_WEATHER:
        refreshWeather();
        break;
      case SCREEN_AI:
        refreshInsight();
        break;
      default:
        break;
    }
    sDirty = true;
  }

  if (btnSel.pressed()) {
    if (sScreen == SCREEN_MENU) {
      sScreen = static_cast<Screen>(SCREEN_CLOCK + sMenuIndex);
      if (sScreen == SCREEN_AI && sInsight[0] == '\0') refreshInsight();
    } else {
      sScreen = SCREEN_MENU;
    }
    sDirty = true;
  }

  const unsigned long now = millis();
  if (netOnline()) {
    if (!netTimeReady() || (long)(now - sNextTimeSync) >= 0) {
      netSyncTime();
      sNextTimeSync = now + 3600000UL;
      sDirty = true;
    }
    if ((long)(now - sNextWeather) >= 0) refreshWeather();
    if ((long)(now - sNextInsight) >= 0 && sWeather.valid) refreshInsight();
  }

  if (sScreen == SCREEN_CLOCK) {
    struct tm t;
    if (getLocalTime(&t, 10) && t.tm_sec != sLastSecond) {
      sLastSecond = t.tm_sec;
      sDirty = true;
    }
  }

  if (sDirty) render();
  delay(20);
}
