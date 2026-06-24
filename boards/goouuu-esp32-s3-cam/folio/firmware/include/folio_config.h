#pragma once

// Node capture + camera — override via platformio.ini build_flags (-DNAME=value).
// See README — brain syncs most settings at runtime.

#ifndef FOLIO_SAMPLE_RATE
#define FOLIO_SAMPLE_RATE 16000
#endif

#ifndef FOLIO_CHUNK_MS
#define FOLIO_CHUNK_MS 1000
#endif

#define FOLIO_CHUNK_SAMPLES (FOLIO_SAMPLE_RATE * FOLIO_CHUNK_MS / 1000)
#define FOLIO_CHUNK_BYTES (FOLIO_CHUNK_SAMPLES * sizeof(int16_t))

#ifndef FOLIO_FRAME_INTERVAL_MS
#define FOLIO_FRAME_INTERVAL_MS 60000
#endif

#ifndef FOLIO_JPEG_QUALITY
#define FOLIO_JPEG_QUALITY 12
#endif

/** esp_camera framesize_t: 5=CIF, 6=QVGA, 7=VGA, 8=SVGA, 9=XGA */
#ifndef FOLIO_FRAME_SIZE_ID
#define FOLIO_FRAME_SIZE_ID 6
#endif

#ifndef FOLIO_WIFI_RETRY_MS
#define FOLIO_WIFI_RETRY_MS 5000
#endif

#ifndef FOLIO_PUSH_BACKOFF_MAX_MS
#define FOLIO_PUSH_BACKOFF_MAX_MS 30000
#endif

#ifndef FOLIO_STATUS_INTERVAL_MS
#define FOLIO_STATUS_INTERVAL_MS 15000
#endif

/** RMS gate — 0 until brain calibrates via /api/node/config. */
#ifndef FOLIO_SPEECH_ENERGY
#define FOLIO_SPEECH_ENERGY 0.0f
#endif

/** Non-speech gate — synced from brain calibration. */
#ifndef FOLIO_SOUND_ENERGY
#define FOLIO_SOUND_ENERGY 0.0f
#endif

/** Motion gate — synced from brain calibration. */
#ifndef FOLIO_MOTION_MIN
#define FOLIO_MOTION_MIN 0.0f
#endif
