#pragma once

#include <Arduino.h>

#include "display.h"
#include "net.h"
#include "screens.h"

void uiBegin();
void uiTask();

void uiSplash(const char *line1, const char *line2);
void uiShowProvisioning(bool armed);
void uiShowDash();

void uiTurnPage(int delta);
int uiPage();

void uiSetNight(bool on);
bool uiIsNight();

void uiUpdateClock(const struct tm &now, bool timeReady);
void uiUpdateSystem();
void uiUpdateTimer(int remainingSec, int totalSec, bool breakMode, bool running);
void uiBusy(bool on);

void uiUpdateDash(const Dash &d);

void uiUpdatePlace(const char *city);
void uiUpdateWeather(const Weather &w);
void uiUpdateAir(const Air &a, const Weather &w);
void uiUpdateMoon(const Moon &m);
void uiUpdateWind(const Weather &w);
void uiUpdateRain(const Weather &w);
void uiUpdateSun(const Weather &w);
void uiUpdateNews(const News &n);
void uiUpdateMarket(const Market &m);
void uiUpdateRates(const Rates &r);
void uiUpdateHoliday(const Holiday &h);
void uiUpdateHistory(const History &h);
void uiUpdateSpace(const Space &s);
void uiUpdateDev(const DevStats &d);
void uiUpdateInsight(const char *text, bool pending);
