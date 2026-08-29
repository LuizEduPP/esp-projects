#pragma once

#include <Arduino.h>

#include "dash_config.h"

struct Forecast {
  float minC = 0;
  float maxC = 0;
  int code = -1;
  int weekday = 0;
};

struct Weather {
  bool valid = false;
  float tempC = 0;
  float feelsC = 0;
  float minC = 0;
  float maxC = 0;
  int humidity = 0;
  float windKph = 0;
  int rainProb = 0;
  int code = -1;
  const char *desc = "--";

  Forecast days[3];
  int dayCount = 0;

  float hourly[24] = {0};
  int hourlyCount = 0;
  int hourNow = 0;

  char sunrise[6] = "--:--";
  char sunset[6] = "--:--";
  float uvMax = 0;
};

struct Air {
  bool valid = false;
  int aqi = 0;
  float pm25 = 0;
  float pm10 = 0;
  const char *label = "--";
};

struct Moon {
  float age = 0;
  float illum = 0;
  bool waxing = true;
  const char *name = "--";
};

struct Place {
  bool valid = false;
  char city[24] = DASH_CITY;
  float lat = DASH_LAT;
  float lon = DASH_LON;
  long tzOffsetSec = 0;
};

enum NetState { NET_OFFLINE, NET_CONNECTING, NET_PROVISIONING, NET_ONLINE };

void netBegin();
NetState netState();
bool netOnline();
void netLoop();

void netStartProvisioning();
void netForgetCredentials();
const char *netSsid();
int netRssiBars();
const char *netIp();

bool netTimeReady();
void netSyncTime();
void netApplyTimezone(long offsetSec);

bool netResolvePlace(Place &out);
bool netFetchWeather(Place &place, Weather &out);
bool netFetchAir(const Place &place, Air &out);
void netMoonPhase(time_t when, Moon &out);
bool netFetchInsight(const Place &p, const Weather &w, char *out, size_t outLen);
