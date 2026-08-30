#pragma once

#include <Arduino.h>

#include "dash_config.h"

struct Forecast {
  char day[8] = "--";
  float minC = 0;
  float maxC = 0;
  int code = -1;
};

struct Weather {
  bool valid = false;
  float tempC = 0;
  float feelsC = 0;
  float minC = 0;
  float maxC = 0;
  int humidity = 0;
  float windKph = 0;
  int windDir = 0;
  float gustKph = 0;
  float pressure = 0;
  float pressureDelta = 0;
  int rainProb = 0;
  int code = -1;
  char desc[24] = "--";
  float uvMax = 0;
  char sunrise[6] = "--:--";
  char sunset[6] = "--:--";

  float hourly[24] = {0};
  int hourlyCount = 0;
  int hourNow = 0;

  float rain15[8] = {0};
  int rain15Count = 0;
  int rainStartsInMin = -1;

  Forecast days[3];
  int dayCount = 0;
};

struct Air {
  bool valid = false;
  int aqi = 0;
  float pm25 = 0;
  float pm10 = 0;
  char label[16] = "--";
};

struct Moon {
  float illum = 0;
  bool waxing = true;
  char name[20] = "--";
};

struct News {
  bool valid = false;
  char items[3][112] = {{0}};
  int count = 0;
};

struct Market {
  bool valid = false;
  float usd = 0;
  float usdPct = 0;
  float eur = 0;
  float eurPct = 0;
  float btc = 0;
  float btcPct = 0;
};

struct Rates {
  bool valid = false;
  float selic = 0;
  float cdi = 0;
  float ipca = 0;
};

struct Holiday {
  bool valid = false;
  char name[52] = "--";
  char date[11] = "--";
  int daysLeft = 0;
};

struct History {
  bool valid = false;
  int year = 0;
  char text[184] = "--";
};

struct Space {
  bool valid = false;
  int people = 0;
  float lat = 0;
  float lon = 0;
};

struct DevStats {
  bool valid = false;
  int today = 0;
  int week = 0;
  int activeDays = 0;
  int repos = 0;
  int followers = 0;
  char repo[32] = "--";
};

struct Dash {
  bool valid = false;
  char city[28] = DASH_CITY;
  long tzOffsetSec = 0;
  Weather weather;
  Air air;
  Moon moon;
  News news;
  Market market;
  Rates rates;
  Holiday holiday;
  History history;
  Space space;
  DevStats dev;
  char ai[DASH_AI_TEXT_MAX] = "";
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

bool netFetchDash(Dash &out);
