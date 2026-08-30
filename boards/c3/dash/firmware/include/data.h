#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

JsonDocument &dataDoc();

bool dataValid();
void dataMarkValid(bool valid);

void dataUpdateClock(bool timeReady);
void dataUpdateSystem();
void dataUpdateSun();

JsonVariantConst dataAt(const char *path);

void dataFormat(const char *path, const char *fmt, char *out, size_t len);
