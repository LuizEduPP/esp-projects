#pragma once

#include <stdint.h>
#include <stdbool.h>

#define FOLIO_SAMPLE_RATE     16000
#define FOLIO_CHUNK_MS        1000
#define FOLIO_CHUNK_SAMPLES   (FOLIO_SAMPLE_RATE * FOLIO_CHUNK_MS / 1000)
#define FOLIO_CHUNK_BYTES     (FOLIO_CHUNK_SAMPLES * sizeof(int16_t))

bool audioBegin();
void audioEnd();
bool audioReadChunk(int16_t *out, uint32_t sampleCount);
