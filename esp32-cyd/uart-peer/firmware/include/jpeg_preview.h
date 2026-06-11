#pragma once

#include <Arduino.h>
#include "display_cyd.hpp"

void jpegPreviewBegin(LGFX_CYD &tft);
bool jpegPreviewShow(LGFX_CYD &tft, const uint8_t *data, size_t len);
void jpegPreviewDrawStatus(LGFX_CYD &tft, const char *line1, const char *line2 = nullptr);
