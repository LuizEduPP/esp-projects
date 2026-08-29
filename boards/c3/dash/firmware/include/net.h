#pragma once

#include <Arduino.h>

#include "dash_config.h"

struct Weather {
  bool valid = false;
  float tempC = 0;
  int humidity = 0;
  float windKph = 0;
  int code = -1;
  const char *desc = "--";
};

void netBegin();
bool netEnsure();
bool netOnline();
bool netTimeReady();
void netSyncTime();
bool netFetchWeather(Weather &out);
bool netFetchInsight(const Weather &w, char *out, size_t outLen);
