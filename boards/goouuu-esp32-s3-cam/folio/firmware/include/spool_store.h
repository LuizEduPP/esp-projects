#pragma once

#include <stdint.h>
#include <stddef.h>

#include "audio_capture.h"

#ifndef FOLIO_MICROSD_SPOOL
#define FOLIO_MICROSD_SPOOL 1
#endif

bool spoolBegin();
bool spoolOk();
/** Re-mount microSD if needed. Required when FOLIO_MICROSD_SPOOL=1. */
bool spoolEnsure();
void spoolTick();
bool spoolRequired();

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
