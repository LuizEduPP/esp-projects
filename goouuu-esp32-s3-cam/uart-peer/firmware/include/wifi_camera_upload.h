#pragma once

#include <Arduino.h>

bool wifiCameraUploadFrame(const uint8_t *data, size_t len, int frameId, bool logToSerial);
