#pragma once

#include <Arduino.h>

// Intervalo alvo do stream (S3 loop + cooldown da câmera).
#define STREAM_INTERVAL_MS 1000

bool cameraCaptureBegin();
bool cameraCaptureRaw(const uint8_t **outBuf, size_t *outLen);
void cameraCaptureRelease();
bool cameraCaptureReady();
