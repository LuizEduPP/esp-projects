#pragma once

#include "display_cyd.hpp"

inline void displaySetup(LGFX_CYD &tft) {
  tft.init();
  tft.setRotation(CYD_TFT_ROTATION);
  tft.setBrightness(255);
  tft.fillScreen(TFT_BLACK);
}

inline void displayEnsureLandscape(LGFX_CYD &tft) {
  tft.setRotation(CYD_TFT_ROTATION);
}

// Só dispara na transição dedo-down (evita repetição / piscar enquanto pressionado).
inline bool touchReadDown(LGFX_CYD &tft, int16_t &x, int16_t &y) {
  static bool wasDown = false;
  const bool down = tft.getTouch(&x, &y);
  if (down && !wasDown) {
    wasDown = true;
    return true;
  }
  if (!down) {
    wasDown = false;
  }
  return false;
}

inline bool touchRead(LGFX_CYD &tft, int16_t &x, int16_t &y) {
  return touchReadDown(tft, x, y);
}
