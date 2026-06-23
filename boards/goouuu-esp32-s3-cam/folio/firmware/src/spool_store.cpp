#include "spool_store.h"

#include <SD_MMC.h>

#include "pins.h"

// Paths must be under the SD_MMC mount point (/sdcard).
static const char *SPOOL_ROOT = "/sdcard/folio";
static const char *SPOOL_AUDIO = "/sdcard/folio/audio";
static const char *SPOOL_FRAMES = "/sdcard/folio/frames";

static bool gSpoolOk = false;
static unsigned long gLastMountTryMs = 0;

static bool cardMounted() { return SD_MMC.cardType() != CARD_NONE; }

static void spoolPath(char *out, size_t outLen, const char *dir, uint32_t id,
                      const char *ext) {
  snprintf(out, outLen, "%s/%08lu.%s", dir, (unsigned long)id, ext);
}

static bool writeFile(const char *path, const uint8_t *data, size_t len) {
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    return false;
  }
  const size_t n = f.write(data, len);
  f.close();
  return n == len;
}

static bool writeText(const char *path, const char *text) {
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
  File f = SD_MMC.open(path, FILE_WRITE);
  if (!f) {
    return false;
  }
  const size_t n = f.print(text);
  f.close();
  return n == strlen(text);
}

static bool readText(const char *path, char *out, size_t metaLen) {
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    return false;
  }
  size_t n = f.readBytes(out, metaLen - 1);
  out[n] = '\0';
  f.close();
  return true;
}

static bool deletePair(const char *dir, uint32_t id, const char *dataExt) {
  if (!gSpoolOk || !cardMounted()) {
    return false;
  }
  char path[80];
  spoolPath(path, sizeof(path), dir, id, dataExt);
  SD_MMC.remove(path);
  spoolPath(path, sizeof(path), dir, id, "meta");
  SD_MMC.remove(path);
  return true;
}

static bool oldestInDir(const char *dir, const char *dataExt, uint32_t *outId) {
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
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
  if (!gSpoolOk || !cardMounted()) {
    return 0;
  }
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
  if (cardMounted()) {
    SD_MMC.end();
  }
  if (!SD_MMC.begin(SD_MOUNT, true)) {
    gSpoolOk = false;
    return false;
  }
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
  SD_MMC.mkdir(SPOOL_ROOT);
  SD_MMC.mkdir(SPOOL_AUDIO);
  SD_MMC.mkdir(SPOOL_FRAMES);
  gSpoolOk = true;
  Serial.printf("[spool] SD ok %lluMB path=%s\n", SD_MMC.cardSize() / 1048576ULL, SPOOL_ROOT);
  return true;
}

bool spoolOk() { return gSpoolOk && cardMounted(); }

bool spoolEnsure() {
  if (spoolOk()) {
    return true;
  }
  return spoolBegin();
}

void spoolTick() {
  if (spoolOk()) {
    return;
  }
  const unsigned long now = millis();
  if (now - gLastMountTryMs < 30000) {
    return;
  }
  gLastMountTryMs = now;
  if (spoolBegin()) {
    Serial.println("[spool] SD re-mounted");
  }
}

bool spoolSaveAudio(uint32_t seq, const int16_t *pcm, const char *meta) {
  if (!spoolEnsure()) {
    return false;
  }
  char path[80];
  spoolPath(path, sizeof(path), SPOOL_AUDIO, seq, "pcm");
  if (!writeFile(path, reinterpret_cast<const uint8_t *>(pcm), FOLIO_CHUNK_BYTES)) {
    gSpoolOk = false;
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_AUDIO, seq, "meta");
  if (!writeText(path, meta)) {
    gSpoolOk = false;
    return false;
  }
  return true;
}

bool spoolSaveFrame(uint32_t id, const uint8_t *jpeg, size_t len, const char *meta) {
  if (!spoolEnsure()) {
    return false;
  }
  char path[80];
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "jpg");
  if (!writeFile(path, jpeg, len)) {
    gSpoolOk = false;
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "meta");
  if (!writeText(path, meta)) {
    gSpoolOk = false;
    return false;
  }
  return true;
}

bool spoolDeleteAudio(uint32_t seq) { return deletePair(SPOOL_AUDIO, seq, "pcm"); }

bool spoolDeleteFrame(uint32_t id) { return deletePair(SPOOL_FRAMES, id, "jpg"); }

bool spoolOldestAudio(uint32_t *seq, char *metaOut, size_t metaLen) {
  if (!spoolOk() || !oldestInDir(SPOOL_AUDIO, ".pcm", seq)) {
    return false;
  }
  char path[80];
  spoolPath(path, sizeof(path), SPOOL_AUDIO, *seq, "meta");
  readText(path, metaOut, metaLen);
  return true;
}

bool spoolOldestFrame(uint32_t *id, char *metaOut, size_t metaLen) {
  if (!spoolOk() || !oldestInDir(SPOOL_FRAMES, ".jpg", id)) {
    return false;
  }
  char path[80];
  spoolPath(path, sizeof(path), SPOOL_FRAMES, *id, "meta");
  readText(path, metaOut, metaLen);
  return true;
}

bool spoolReadAudio(uint32_t seq, int16_t *pcmOut, char *metaOut, size_t metaLen) {
  if (!spoolOk()) {
    return false;
  }
  char path[80];
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
  if (!spoolOk()) {
    return false;
  }
  char path[80];
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
