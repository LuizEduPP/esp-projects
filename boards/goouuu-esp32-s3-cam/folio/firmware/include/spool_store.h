#pragma once

#include <stdint.h>
#include <stddef.h>

#include "audio_capture.h"

bool spoolBegin();
bool spoolOk();

bool spoolSaveAudio(uint32_t seq, const int16_t *pcm, const char *meta);
bool spoolSaveFrame(uint32_t id, const uint8_t *jpeg, size_t len, const char *meta);

bool spoolDeleteAudio(uint32_t seq);
bool spoolDeleteFrame(uint32_t id);

bool spoolOldestAudio(uint32_t *seq, char *metaOut, size_t metaLen);
bool spoolOldestFrame(uint32_t *id, char *metaOut, size_t metaLen);

bool spoolReadAudio(uint32_t seq, int16_t *pcmOut, char *metaOut, size_t metaLen);
bool spoolReadFrame(uint32_t id, uint8_t **jpegOut, size_t *lenOut, char *metaOut,
                    size_t metaLen);

void spoolFreeBuffer(uint8_t *buf);

uint32_t spoolPendingAudio();
uint32_t spoolPendingFrames();
