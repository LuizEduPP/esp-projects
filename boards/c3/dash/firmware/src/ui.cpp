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

void uiUpdatePlace(const char *city) { screensCity(city); }

void uiUpdateWeather(const Weather &w) {
  if (!w.valid) {
    screensWeather(0, "sem dados", 0, 0, 0, -1);
    return;
  }

  screensWeather(w.tempC, w.desc, w.humidity, w.minC, w.maxC, w.code);

  for (int i = 0; i < 3; ++i) {
    if (i < w.dayCount) {
      const Forecast &f = w.days[i];
      screensForecast(i, f.day, f.minC, f.maxC, f.code);
    } else {
      screensForecast(i, "--", 0, 0, -1);
    }
  }

  if (w.hourlyCount > 0) screensChart(w.hourly, w.hourlyCount);
  screensSun(w.sunrise, w.sunset);
  uiUpdateWind(w);
  uiUpdateRain(w);
  uiUpdateSun(w);
}

void uiUpdateAir(const Air &a, const Weather &w) {
  if (!a.valid) {
    screensAir(0, "sem dados", 0, 0, w.uvMax);
    return;
  }
  screensAir(a.aqi, a.label, a.pm25, a.pm10, w.uvMax);
}

void uiUpdateMoon(const Moon &m) { screensMoon(m.name, m.illum, m.waxing); }

void uiUpdateWind(const Weather &w) {
  screensWind(w.windKph, w.windDir, w.gustKph, w.pressure, w.pressureDelta);
}

void uiUpdateRain(const Weather &w) {
  screensRain(w.rain15, w.rain15Count, w.rainStartsInMin);
}

void uiUpdateSun(const Weather &w) {
  int rh = 0;
  int rm = 0;
  int sh = 0;
  int sm = 0;
  if (sscanf(w.sunrise, "%d:%d", &rh, &rm) != 2 ||
      sscanf(w.sunset, "%d:%d", &sh, &sm) != 2) {
    screensSun2(w.sunrise, w.sunset, "--", 0);
    return;
  }

  const int rise = rh * 60 + rm;
  const int set = sh * 60 + sm;
  const int span = set - rise;

  char daylight[24];
  snprintf(daylight, sizeof(daylight), "%dh %02dmin de sol", span / 60, span % 60);

  int progress = 0;
  struct tm now;
  if (getLocalTime(&now, 20) && span > 0) {
    const int mins = now.tm_hour * 60 + now.tm_min;
    progress = (mins - rise) * 100 / span;
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;
  }

  screensSun2(w.sunrise, w.sunset, daylight, progress);
}

void uiUpdateMarket(const Market &m) {
  if (!m.valid) return;
  screensMarket(m.usd, m.usdPct, m.eur, m.eurPct, m.btc, m.btcPct);
}

void uiUpdateDev(const DevStats &d) {
  if (!d.valid) {
    screensDev(0, 0, 0, 0, 0, "sem dados");
    return;
  }
  screensDev(d.today, d.week, d.activeDays, d.repos, d.followers, d.repo);
}

void uiUpdateNews(const News &n) {
  screensNews(n.items[0], n.items[1], n.items[2], n.count);
}

void uiUpdateRates(const Rates &r) {
  if (r.valid) screensRates(r.selic, r.cdi, r.ipca);
}

void uiUpdateHoliday(const Holiday &h) {
  if (h.valid) screensHoliday(h.name, h.date, h.daysLeft);
}

void uiUpdateHistory(const History &h) {
  if (h.valid) screensHistory(h.year, h.text);
}

void uiUpdateSpace(const Space &s) {
  if (s.valid) screensSpace(s.people, s.lat, s.lon);
}

void uiUpdateDash(const Dash &d) {
  uiUpdatePlace(d.city);
  uiUpdateWeather(d.weather);
  uiUpdateAir(d.air, d.weather);
  uiUpdateMoon(d.moon);
  uiUpdateNews(d.news);
  uiUpdateMarket(d.market);
  uiUpdateRates(d.rates);
  uiUpdateHoliday(d.holiday);
  uiUpdateHistory(d.history);
  uiUpdateSpace(d.space);
  uiUpdateDev(d.dev);
  uiUpdateInsight(d.ai[0] ? d.ai : "sem resposta", false);
}

void uiBusy(bool on) {
  screensBusy(on);
  displayTask();
}

void uiUpdateTimer(int remainingSec, int totalSec, bool breakMode, bool running) {
  char clock[8];
  snprintf(clock, sizeof(clock), "%02d:%02d", remainingSec / 60, remainingSec % 60);
  const int percent = totalSec > 0 ? (totalSec - remainingSec) * 100 / totalSec : 0;
  screensTimer(clock, breakMode ? "pausa" : "foco", percent, running);
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
