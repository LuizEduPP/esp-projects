#pragma once

#include <Arduino.h>

#include "display_cyd.hpp"

void wifiCameraFetchBegin(LGFX_CYD &tft);
void wifiCameraFetchLoop(bool wifiConnected, bool streamEnabled);
void wifiCameraFetchKick();
