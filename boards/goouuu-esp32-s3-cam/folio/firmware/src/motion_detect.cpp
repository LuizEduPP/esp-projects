#include "motion_detect.h"

#ifndef FOLIO_MOTION_MIN
#define FOLIO_MOTION_MIN 0.08f
#endif

static uint32_t gPrevSig = 0;

static uint32_t fnv1aSample(const uint8_t *data, size_t len) {
  uint32_t h = 2166136261u;
  if (!data || len == 0) {
    return h;
  }
  const size_t step = len > 2048 ? len / 256 : 4;
  for (size_t i = 0; i < len; i += step) {
    h ^= data[i];
    h *= 16777619u;
  }
  return h;
}

static int popcount32(uint32_t v) {
  int c = 0;
  while (v) {
    c += v & 1;
    v >>= 1;
  }
  return c;
}

float motionScoreJpeg(const uint8_t *jpeg, size_t len, bool *changed) {
  const uint32_t sig = fnv1aSample(jpeg, len);
  if (gPrevSig == 0) {
    gPrevSig = sig;
    if (changed) {
      *changed = true;
    }
    return 1.0f;
  }
  const int bits = popcount32(gPrevSig ^ sig);
  gPrevSig = sig;
  const float score = bits / 32.0f;
  if (changed) {
    *changed = score >= FOLIO_MOTION_MIN;
  }
  return score;
}
