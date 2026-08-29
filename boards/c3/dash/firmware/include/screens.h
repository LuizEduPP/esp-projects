#pragma once

#include <stdbool.h>

#define UI_PAGES 8

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
