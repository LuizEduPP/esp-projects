#pragma once

#include <Arduino.h>

/** Scan for open WiFi, else optional configured SSID (blocks up to ~15s). */
void folioWifiBegin();

/** Rescan / reconnect when offline (call from loop). */
void folioWifiMaintain(uint32_t retryMs);

bool folioWifiConnected();

/** SSID of current association (empty if offline). */
const char *folioWifiSsid();
