#pragma once

#include <Arduino.h>

bool wifiStoreLoad(String &ssid, String &pass, String &serverUrl);
bool wifiStoreSave(const String &ssid, const String &pass, const String &serverUrl);
bool wifiStoreHasCredentials();
void wifiStoreApplySecretsFallback(String &ssid, String &pass, String &serverUrl);
