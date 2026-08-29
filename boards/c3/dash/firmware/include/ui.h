#pragma once

#include <Arduino.h>

#include "display.h"
#include "net.h"

#define UI_PAGES 6

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
void uiUpdateWeather(const Weather &w);
void uiUpdateInsight(const char *text, bool pending);
void uiUpdateSystem();
