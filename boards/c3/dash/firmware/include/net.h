#pragma once

#include <Arduino.h>

#include "dash_config.h"

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

bool netGeolocate(Place &out);
bool netFetchWeather(Place &place, Weather &out);
bool netFetchInsight(const Place &p, const Weather &w, char *out, size_t outLen);
