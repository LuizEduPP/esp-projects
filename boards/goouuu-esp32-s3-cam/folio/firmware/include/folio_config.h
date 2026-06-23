#pragma once

// Node capture + camera — override via platformio.ini build_flags (-DNAME=value).
// See folio.config.example.json and README.

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

/** RMS energy gate — must match brain audio.speechEnergyThreshold (default 0.008). */
#ifndef FOLIO_SPEECH_ENERGY
#define FOLIO_SPEECH_ENERGY 0.008f
#endif

/** Lower gate for non-speech sounds (latido, porta…) — match brain perception.soundMinEnergy. */
#ifndef FOLIO_SOUND_ENERGY
#define FOLIO_SOUND_ENERGY 0.003f
#endif

/** JPEG motion proxy threshold (0..1) — synced from brain perception.motionMin at runtime. */
#ifndef FOLIO_MOTION_MIN
#define FOLIO_MOTION_MIN 0.08f
#endif
