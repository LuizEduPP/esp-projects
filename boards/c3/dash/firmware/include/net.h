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

  int windDir = 0;
  float gustKph = 0;
  float pressure = 0;
  float pressureDelta = 0;

  float rain15[8] = {0};
  int rain15Count = 0;
  int rainStartsInMin = -1;
};

struct Market {
  bool valid = false;
  float usd = 0;
  float eur = 0;
  float btc = 0;
  float usdPct = 0;
  float eurPct = 0;
  float btcPct = 0;
};

struct DevStats {
  bool valid = false;
  int commitsToday = 0;
  int commitsWeek = 0;
  int pushes = 0;
  int prs = 0;
  int issues = 0;
  int repos = 0;
  int followers = 0;
  int activeDays = 0;
  int activeRepos = 0;
  char lastRepo[32] = "--";
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

struct News {
  bool valid = false;
  char items[3][96] = {{0}};
  int count = 0;
};

struct Holiday {
  bool valid = false;
  char name[40] = "--";
  char date[11] = "--";
  int daysLeft = 0;
};

struct Rates {
  bool valid = false;
  float selic = 0;
  float cdi = 0;
  float ipca = 0;
};

struct History {
  bool valid = false;
  int year = 0;
  char text[160] = "--";
};

struct Space {
  bool valid = false;
  int people = 0;
  float issLat = 0;
  float issLon = 0;
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
bool netFetchMarket(Market &out);
bool netFetchDev(DevStats &out);
bool netFetchNews(News &out);
bool netFetchHoliday(Holiday &out);
bool netFetchRates(Rates &out);
bool netFetchHistory(History &out);
bool netFetchSpace(Space &out);
void netMoonPhase(time_t when, Moon &out);
bool netFetchInsight(const Place &p, const Weather &w, char *out, size_t outLen);
