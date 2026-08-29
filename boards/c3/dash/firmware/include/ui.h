#pragma once

#include <Arduino.h>

#include "net.h"

#define UI_W 128
#define UI_H 128

void uiBegin();
void uiPush();
void uiMenu(const char *const *items, int count, int selected, bool online);
void uiClock(const struct tm &now, bool timeReady, bool online);
void uiWeather(const Weather &w, bool online);
void uiInsight(const char *text, bool pending, bool online);
void uiSplash(const char *line1, const char *line2);
