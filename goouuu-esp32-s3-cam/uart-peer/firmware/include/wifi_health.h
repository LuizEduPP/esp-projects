#pragma once

#include <Arduino.h>

void wifiHealthBegin(const char *deviceId, const char *role, bool logToSerial);
void wifiHealthLoop(bool pairReady, uint32_t uptimeSec, bool otaAllowed = true);
void wifiHealthReload(bool logToSerial);
bool wifiHealthConnected();
bool wifiHealthEnabled();
