#pragma once

#include <Arduino.h>

typedef void (*WifiOtaUiFn)(const char *message);

void wifiOtaSetUiCallback(WifiOtaUiFn fn);
bool wifiOtaApplyFromHealthJson(const String &healthBody, bool logToSerial);
bool wifiOtaApplyUrl(const char *url, const char *version, bool logToSerial);
