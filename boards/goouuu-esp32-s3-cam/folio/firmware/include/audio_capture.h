#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "folio_config.h"

bool audioBegin();
void audioEnd();
bool audioReadChunk(int16_t *out, uint32_t sampleCount);
