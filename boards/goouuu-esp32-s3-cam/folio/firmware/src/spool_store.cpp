#include "spool_store.h"

#include <SD_MMC.h>

#include "pins.h"

static const char *SPOOL_ROOT = "/folio";
static const char *SPOOL_AUDIO = "/folio/audio";
static const char *SPOOL_FRAMES = "/folio/frames";

static bool gSpoolOk = false;

static void spoolPath(char *out, size_t outLen, const char *dir, uint32_t id,
                      const char *ext) {
  snprintf(out, outLen, "%s/%08lu.%s", dir, (unsigned long)id, ext);
}

static bool writeFile(const char *path, const uint8_t *data, size_t len) {
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    return false;
  }
  const size_t n = f.write(data, len);
  f.close();
  return n == len;
}

static bool writeText(const char *path, const char *text) {
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    return false;
  }
  const size_t n = f.print(text);
  f.close();
  return n == strlen(text);
}

static bool readText(const char *path, char *out, size_t outLen) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  size_t n = f.readBytes(out, outLen - 1);
  out[n] = '\0';
  f.close();
  return true;
}

static bool deletePair(const char *dir, uint32_t id, const char *dataExt) {
  char path[64];
  spoolPath(path, sizeof(path), dir, id, dataExt);
  SD_MMC.remove(path);
  spoolPath(path, sizeof(path), dir, id, "meta");
  SD_MMC.remove(path);
  return true;
}

static bool oldestInDir(const char *dir, const char *dataExt, uint32_t *outId) {
  File root = SD_MMC.open(dir);
  if (!root || !root.isDirectory()) {
    return false;
  }

  uint32_t best = UINT32_MAX;
  File entry;
  while ((entry = root.openNextFile())) {
    if (entry.isDirectory()) {
      entry.close();
      continue;
    }
    String name = entry.name();
    entry.close();
    if (!name.endsWith(dataExt)) {
      continue;
    }
    const int dot = name.lastIndexOf('.');
    if (dot <= 0) {
      continue;
    }
    const uint32_t id = (uint32_t)name.substring(0, dot).toInt();
    if (id < best) {
      best = id;
    }
  }
  root.close();

  if (best == UINT32_MAX) {
    return false;
  }
  *outId = best;
  return true;
}

static uint32_t countInDir(const char *dir, const char *dataExt) {
  File root = SD_MMC.open(dir);
  if (!root || !root.isDirectory()) {
    return 0;
  }
  uint32_t n = 0;
  File entry;
  while ((entry = root.openNextFile())) {
    if (!entry.isDirectory()) {
      String name = entry.name();
      if (name.endsWith(dataExt)) {
        n++;
      }
    }
    entry.close();
  }
  root.close();
  return n;
}

bool spoolBegin() {
  SD_MMC.setPins(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0);
  if (!SD_MMC.begin(SD_MOUNT, true)) {
    gSpoolOk = false;
    return false;
  }
  SD_MMC.mkdir(SPOOL_ROOT);
  SD_MMC.mkdir(SPOOL_AUDIO);
  SD_MMC.mkdir(SPOOL_FRAMES);
  gSpoolOk = true;
  return true;
}

bool spoolOk() { return gSpoolOk; }

bool spoolSaveAudio(uint32_t seq, const int16_t *pcm, const char *meta) {
  if (!gSpoolOk) {
    return false;
  }
  char path[64];
  spoolPath(path, sizeof(path), SPOOL_AUDIO, seq, "pcm");
  if (!writeFile(path, reinterpret_cast<const uint8_t *>(pcm), FOLIO_CHUNK_BYTES)) {
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_AUDIO, seq, "meta");
  return writeText(path, meta);
}

bool spoolSaveFrame(uint32_t id, const uint8_t *jpeg, size_t len, const char *meta) {
  if (!gSpoolOk) {
    return false;
  }
  char path[64];
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "jpg");
  if (!writeFile(path, jpeg, len)) {
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "meta");
  return writeText(path, meta);
}

bool spoolDeleteAudio(uint32_t seq) {
  if (!gSpoolOk) {
    return false;
  }
  return deletePair(SPOOL_AUDIO, seq, "pcm");
}

bool spoolDeleteFrame(uint32_t id) {
  if (!gSpoolOk) {
    return false;
  }
  return deletePair(SPOOL_FRAMES, id, "jpg");
}

bool spoolOldestAudio(uint32_t *seq, char *metaOut, size_t metaLen) {
  if (!gSpoolOk || !oldestInDir(SPOOL_AUDIO, ".pcm", seq)) {
    return false;
  }
  char path[64];
  spoolPath(path, sizeof(path), SPOOL_AUDIO, *seq, "meta");
  readText(path, metaOut, metaLen);
  return true;
}

bool spoolOldestFrame(uint32_t *id, char *metaOut, size_t metaLen) {
  if (!gSpoolOk || !oldestInDir(SPOOL_FRAMES, ".jpg", id)) {
    return false;
  }
  char path[64];
  spoolPath(path, sizeof(path), SPOOL_FRAMES, *id, "meta");
  readText(path, metaOut, metaLen);
  return true;
}

bool spoolReadAudio(uint32_t seq, int16_t *pcmOut, char *metaOut, size_t metaLen) {
  if (!gSpoolOk) {
    return false;
  }
  char path[64];
  spoolPath(path, sizeof(path), SPOOL_AUDIO, seq, "pcm");
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  const size_t n = f.read(reinterpret_cast<uint8_t *>(pcmOut), FOLIO_CHUNK_BYTES);
  f.close();
  if (n != FOLIO_CHUNK_BYTES) {
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_AUDIO, seq, "meta");
  readText(path, metaOut, metaLen);
  return true;
}

bool spoolReadFrame(uint32_t id, uint8_t **jpegOut, size_t *lenOut, char *metaOut,
                    size_t metaLen) {
  if (!gSpoolOk) {
    return false;
  }
  char path[64];
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "jpg");
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  const size_t len = f.size();
  uint8_t *buf = len ? (uint8_t *)ps_malloc(len) : nullptr;
  if (!buf) {
    f.close();
    return false;
  }
  const size_t n = f.read(buf, len);
  f.close();
  if (n != len) {
    free(buf);
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "meta");
  readText(path, metaOut, metaLen);
  *jpegOut = buf;
  *lenOut = len;
  return true;
}

void spoolFreeBuffer(uint8_t *buf) { free(buf); }

uint32_t spoolPendingAudio() { return countInDir(SPOOL_AUDIO, ".pcm"); }

uint32_t spoolPendingFrames() { return countInDir(SPOOL_FRAMES, ".jpg"); }
