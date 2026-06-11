#pragma once

#include "display_cyd.hpp"

static const int UI_HEADER_H = 18;
static const int UI_FOOTER_H = 20;

inline int uiContentY(const LGFX_CYD &tft) {
  (void)tft;
  return UI_HEADER_H + 1;
}

inline int uiContentH(const LGFX_CYD &tft) {
  return tft.height() - UI_HEADER_H - UI_FOOTER_H - 1;
}

inline int uiFooterY(const LGFX_CYD &tft) {
  return tft.height() - UI_FOOTER_H;
}
