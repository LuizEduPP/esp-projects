#pragma once

#include <stddef.h>
#include <stdint.h>

/** Fast JPEG byte-signature motion proxy (0..1). */
float motionScoreJpeg(const uint8_t *jpeg, size_t len, bool *changed);
