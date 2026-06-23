#include "spool_store.h"

#include <Arduino.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_task_wdt.h>
#include <esp_vfs_fat.h>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>

#include "pins.h"

// microSD slot (SD_MMC 1-bit) — independent from INMP441 I2S DOUT pin.
static const char *SPOOL_ROOT = "/sdcard/folio";
static const char *SPOOL_AUDIO = "/sdcard/folio/audio";
static const char *SPOOL_FRAMES = "/sdcard/folio/frames";

static sdmmc_card_t *gCard = nullptr;
static bool gSpoolOk = false;
static unsigned long gLastMountTryMs = 0;
static int gSdClk = SD_PIN_CLK;
static int gSdCmd = SD_PIN_CMD;
static int gSdD0 = SD_PIN_D0;

static bool cardMounted() { return gCard != nullptr; }

bool spoolRequired() { return FOLIO_MICROSD_SPOOL != 0; }

static void unmountCard() {
  if (!gCard) {
    return;
  }
  esp_vfs_fat_sdcard_unmount(SD_MOUNT, gCard);
  gCard = nullptr;
  gSpoolOk = false;
}

static bool tryMountOnce(int clk, int cmd, int d0, int freqKhz, esp_err_t *outErr) {
  esp_task_wdt_reset();
  unmountCard();
  delay(30);

  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.flags = SDMMC_HOST_FLAG_1BIT;
  host.slot = SDMMC_HOST_SLOT_1;
  host.max_freq_khz = freqKhz;

  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.width = 1;
  slot.clk = static_cast<gpio_num_t>(clk);
  slot.cmd = static_cast<gpio_num_t>(cmd);
  slot.d0 = static_cast<gpio_num_t>(d0);
  slot.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;

  esp_vfs_fat_sdmmc_mount_config_t mountConfig = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024,
  };

  esp_err_t ret = esp_vfs_fat_sdmmc_mount(SD_MOUNT, &host, &slot, &mountConfig, &gCard);
  esp_task_wdt_reset();
  if (outErr) {
    *outErr = ret;
  }
  if (ret != ESP_OK) {
    unmountCard();
    return false;
  }
  gSdClk = clk;
  gSdCmd = cmd;
  gSdD0 = d0;
  return true;
}

static bool mkdirIfNeeded(const char *path) {
  struct stat st;
  if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
    return true;
  }
  return mkdir(path, 0755) == 0 || (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static void spoolPath(char *out, size_t outLen, const char *dir, uint32_t id,
                      const char *ext) {
  snprintf(out, outLen, "%s/%08lu.%s", dir, (unsigned long)id, ext);
}

static bool writeFile(const char *path, const uint8_t *data, size_t len) {
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  const size_t n = fwrite(data, 1, len, f);
  fclose(f);
  return n == len;
}

static bool writeText(const char *path, const char *text) {
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
  FILE *f = fopen(path, "wb");
  if (!f) {
    return false;
  }
  const size_t n = fwrite(text, 1, strlen(text), f);
  fclose(f);
  return n == strlen(text);
}

static bool readText(const char *path, char *out, size_t metaLen) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  const size_t n = fread(out, 1, metaLen - 1, f);
  out[n] = '\0';
  fclose(f);
  return true;
}

static bool deletePair(const char *dir, uint32_t id, const char *dataExt) {
  if (!gSpoolOk || !cardMounted()) {
    return false;
  }
  char path[80];
  spoolPath(path, sizeof(path), dir, id, dataExt);
  unlink(path);
  spoolPath(path, sizeof(path), dir, id, "meta");
  unlink(path);
  return true;
}

static bool oldestInDir(const char *dir, const char *dataExt, uint32_t *outId) {
  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }
  DIR *root = opendir(dir);
  if (!root) {
    return false;
  }

  uint32_t best = UINT32_MAX;
  struct dirent *entry;
  while ((entry = readdir(root)) != nullptr) {
    if (entry->d_type != DT_REG) {
      continue;
    }
    const char *name = entry->d_name;
    const size_t nameLen = strlen(name);
    const size_t extLen = strlen(dataExt);
    if (nameLen <= extLen || strcmp(name + nameLen - extLen, dataExt) != 0) {
      continue;
    }
    char idBuf[16];
    const size_t idLen = nameLen - extLen;
    if (idLen >= sizeof(idBuf)) {
      continue;
    }
    memcpy(idBuf, name, idLen);
    idBuf[idLen] = '\0';
    const uint32_t id = (uint32_t)strtoul(idBuf, nullptr, 10);
    if (id < best) {
      best = id;
    }
  }
  closedir(root);

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
  DIR *root = opendir(dir);
  if (!root) {
    return 0;
  }
  uint32_t n = 0;
  struct dirent *entry;
  while ((entry = readdir(root)) != nullptr) {
    if (entry->d_type != DT_REG) {
      continue;
    }
    const char *name = entry->d_name;
    const size_t nameLen = strlen(name);
    const size_t extLen = strlen(dataExt);
    if (nameLen > extLen && strcmp(name + nameLen - extLen, dataExt) == 0) {
      n++;
    }
  }
  closedir(root);
  return n;
}

bool spoolBegin() {
#if !FOLIO_MICROSD_SPOOL
  return false;
#endif
  esp_log_level_set("vfs_fat_sdmmc", ESP_LOG_NONE);
  esp_log_level_set("sdmmc_req", ESP_LOG_NONE);
  esp_log_level_set("sdmmc_common", ESP_LOG_NONE);

  esp_err_t lastErr = ESP_FAIL;

  // GOOUUU ESP32-S3-CAM only: CLK39 CMD38 D0=40. Never probe GPIO35-37 (onboard PSRAM).
  if (!tryMountOnce(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0, SDMMC_FREQ_PROBING, &lastErr) &&
      !tryMountOnce(SD_PIN_CLK, SD_PIN_CMD, SD_PIN_D0, SDMMC_FREQ_DEFAULT, &lastErr)) {
    gSpoolOk = false;
    if (lastErr == ESP_ERR_TIMEOUT) {
      Serial.printf(
          "[microsd] mount fail err=TIMEOUT — card not responding (empty slot? reseat FAT32 card)\n");
    } else {
      Serial.printf("[microsd] mount fail err=%s — need FAT32 card in slot before power-on\n",
                    esp_err_to_name(lastErr));
    }
    Serial.printf("[microsd] pins CLK%d CMD%d D0%d (GOOUUU onboard slot)\n", SD_PIN_CLK, SD_PIN_CMD,
                  SD_PIN_D0);
    return false;
  }

  if (!cardMounted()) {
    gSpoolOk = false;
    return false;
  }

  if (!mkdirIfNeeded(SPOOL_ROOT) || !mkdirIfNeeded(SPOOL_AUDIO) ||
      !mkdirIfNeeded(SPOOL_FRAMES)) {
    gSpoolOk = false;
    Serial.println("[microsd] mkdir /sdcard/folio failed");
    unmountCard();
    return false;
  }

  gSpoolOk = true;
  const uint64_t sizeMb = ((uint64_t)gCard->csd.capacity * gCard->csd.sector_size) / 1048576ULL;
  Serial.printf("[microsd] ok %lluMB CLK%d CMD%d D0%d /folio\n", sizeMb, gSdClk, gSdCmd, gSdD0);
  return true;
}

bool spoolOk() { return gSpoolOk && cardMounted(); }

bool spoolEnsure() {
#if !FOLIO_MICROSD_SPOOL
  return false;
#endif
  if (spoolOk()) {
    return true;
  }
  const unsigned long now = millis();
  if (now - gLastMountTryMs < 5000) {
    return false;
  }
  gLastMountTryMs = now;
  return spoolBegin();
}

void spoolTick() {
#if !FOLIO_MICROSD_SPOOL
  return;
#endif
  if (spoolOk()) {
    return;
  }
  const unsigned long now = millis();
  if (now - gLastMountTryMs < 15000) {
    return;
  }
  gLastMountTryMs = now;
  if (spoolBegin()) {
    Serial.println("[microsd] re-mounted");
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
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  const size_t n = fread(pcmOut, 1, FOLIO_CHUNK_BYTES, f);
  fclose(f);
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
  FILE *f = fopen(path, "rb");
  if (!f) {
    return false;
  }
  fseek(f, 0, SEEK_END);
  const long len = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (len <= 0) {
    fclose(f);
    return false;
  }
  uint8_t *buf = static_cast<uint8_t *>(ps_malloc(static_cast<size_t>(len)));
  if (!buf) {
    fclose(f);
    return false;
  }
  const size_t n = fread(buf, 1, static_cast<size_t>(len), f);
  fclose(f);
  if (n != static_cast<size_t>(len)) {
    free(buf);
    return false;
  }
  spoolPath(path, sizeof(path), SPOOL_FRAMES, id, "meta");
  readText(path, metaOut, metaLen);
  *jpegOut = buf;
  *lenOut = static_cast<size_t>(len);
  return true;
}

void spoolFreeBuffer(uint8_t *buf) { free(buf); }

uint32_t spoolPendingAudio() { return countInDir(SPOOL_AUDIO, ".pcm"); }

uint32_t spoolPendingFrames() { return countInDir(SPOOL_FRAMES, ".jpg"); }
