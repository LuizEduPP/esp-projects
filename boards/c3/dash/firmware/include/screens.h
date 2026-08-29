#pragma once

#include <stdbool.h>

#define UI_PAGES 14

#define PAGE_CLOCK 0
#define PAGE_WEATHER 1
#define PAGE_FORECAST 2
#define PAGE_CHART 3
#define PAGE_RAIN 4
#define PAGE_WIND 5
#define PAGE_AIR 6
#define PAGE_SUN 7
#define PAGE_MOON 8
#define PAGE_MARKET 9
#define PAGE_DEV 10
#define PAGE_TIMER 11
#define PAGE_AI 12
#define PAGE_SYSTEM 13

#ifdef __cplusplus
extern "C" {
#endif

void screensBuild(void);
void screensGoTo(int page, bool animate);
int screensPage(void);

void screensClock(const char *hhmm, int sec, const char *weekday, const char *date);
void screensWeather(float tempC, const char *desc, int humidity, float minC, float maxC,
                    int code);
void screensForecast(int slot, const char *day, float minC, float maxC, int code);
void screensChart(const float *temps, int count);
void screensSun(const char *sunrise, const char *sunset);
void screensAir(int aqi, const char *label, float pm25, float pm10, float uv);
void screensMoon(const char *phase, float illum, bool waxing);
void screensWind(float kph, int dir, float gust, float pressure, float delta);
void screensRain(const float *mm, int count, int startsInMin);
void screensSun2(const char *sunrise, const char *sunset, const char *daylight, int progress);
void screensMarket(float usd, float usdPct, float eur, float eurPct, float btc, float btcPct);
void screensDev(int commitsToday, int commitsWeek, int activeDays, int repos, int followers,
                const char *repo);
void screensBusy(bool on);
void screensTimer(const char *clock, const char *mode, int percent, bool running);
void screensInsight(const char *text, bool pending, const char *stamp);
void screensSystem(const char *ssid, int rssi, const char *ip, unsigned heapKb,
                   const char *uptime);
void screensCity(const char *city);

void screensShowNight(bool on, const char *hhmm);
void screensShowProvisioning(bool armed);
void screensShowDash(void);
void screensSplash(const char *title, const char *subtitle);

#ifdef __cplusplus
}
#endif
