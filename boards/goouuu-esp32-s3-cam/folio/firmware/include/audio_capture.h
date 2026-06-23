#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "folio_config.h"

bool audioBegin();
bool audioRecover();
bool audioDoutStuck();
void audioEnd();
bool audioReadChunk(int16_t *out, uint32_t sampleCount);
