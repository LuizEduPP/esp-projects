#include "ui.h"

#include <WiFi.h>
#include <lvgl.h>

static const char *const kWeekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
static const char *const kMonths[] = {"janeiro",   "fevereiro", "marco",    "abril",
                                      "maio",      "junho",     "julho",    "agosto",
                                      "setembro",  "outubro",   "novembro", "dezembro"};

static bool sNight = false;
static int sPage = 0;

void uiBegin() {
  displayBegin();
  screensBuild();
}

void uiTask() { displayTask(); }

void uiSplash(const char *line1, const char *line2) {
  screensSplash(line1, line2);
  displayTask();
}

void uiShowProvisioning(bool armed) { screensShowProvisioning(armed); }

void uiShowDash() { screensShowDash(); }

void uiTurnPage(int delta) {
  sPage = (sPage + delta + UI_PAGES) % UI_PAGES;
  screensGoTo(sPage, true);
}

int uiPage() { return sPage; }

void uiSetNight(bool on) {
  if (sNight == on) return;
  sNight = on;
  screensShowNight(on, nullptr);
}

bool uiIsNight() { return sNight; }

void uiUpdateClock(const struct tm &now, bool timeReady) {
  char hhmm[8];
  char date[24];
  if (timeReady) {
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
    snprintf(date, sizeof(date), "%d de %s", now.tm_mday, kMonths[now.tm_mon % 12]);
  } else {
    snprintf(hhmm, sizeof(hhmm), "--:--");
    snprintf(date, sizeof(date), "sincronizando");
  }
  screensClock(hhmm, timeReady ? now.tm_sec : 0,
               timeReady ? kWeekdays[now.tm_wday % 7] : "--", date);
}

void uiUpdatePlace(const Place &p) { screensCity(p.city); }

void uiUpdateWeather(const Weather &w) {
  if (!w.valid) {
    screensWeather(0, "sem dados", 0, 0, 0, -1);
    return;
  }

  screensWeather(w.tempC, w.desc, w.humidity, w.minC, w.maxC, w.code);

  for (int i = 0; i < 3; ++i) {
    if (i < w.dayCount) {
      const Forecast &f = w.days[i];
      screensForecast(i, f.weekday >= 0 ? kWeekdays[f.weekday] : "--", f.minC, f.maxC, f.code);
    } else {
      screensForecast(i, "--", 0, 0, -1);
    }
  }

  if (w.hourlyCount > 0) screensChart(w.hourly, w.hourlyCount);
  screensSun(w.sunrise, w.sunset);
}

void uiUpdateAir(const Air &a, const Weather &w) {
  if (!a.valid) {
    screensAir(0, "sem dados", 0, 0, w.uvMax);
    return;
  }
  screensAir(a.aqi, a.label, a.pm25, a.pm10, w.uvMax);
}

void uiUpdateMoon() {
  Moon m;
  netMoonPhase(time(nullptr), m);
  screensMoon(m.name, m.illum, m.waxing);
}

void uiUpdateInsight(const char *text, bool pending) {
  char stamp[8] = "--:--";
  struct tm now;
  if (getLocalTime(&now, 5)) snprintf(stamp, sizeof(stamp), "%02d:%02d", now.tm_hour, now.tm_min);
  screensInsight(text, pending, stamp);
}

void uiUpdateSystem() {
  char uptime[16];
  const unsigned long up = millis() / 1000;
  snprintf(uptime, sizeof(uptime), "%luh %02lum", up / 3600, (up % 3600) / 60);
  screensSystem(netSsid(), (int)WiFi.RSSI(), netIp(),
                (unsigned)(ESP.getFreeHeap() / 1024), uptime);
}
