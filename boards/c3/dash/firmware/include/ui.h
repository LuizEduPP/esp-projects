#pragma once

#include <Arduino.h>

#include "net.h"

#define UI_W 128
#define UI_H 128
#define UI_PAGES 4

void uiBegin();
void uiPush();
void uiScreenPower(bool on);

void uiSplash(const char *line1, const char *line2);
void uiProvisioning(bool armed);
void uiClock(const struct tm &now, bool timeReady, int page);
void uiWeather(const Place &p, const Weather &w, int page);
void uiInsight(const char *text, bool pending, int page);
void uiSystem(const Place &p, int page);
