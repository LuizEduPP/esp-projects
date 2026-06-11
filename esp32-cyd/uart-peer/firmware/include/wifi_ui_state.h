#pragma once

#include "display_cyd.hpp"

void wifiUiStateBegin(const char *deviceId);
void wifiUiStateLoop(bool wifiConnected);
void wifiUiStateDrawOverlay(LGFX_CYD &tft);
bool wifiUiStateTakeRedraw();
