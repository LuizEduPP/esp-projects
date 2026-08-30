#include "ota.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>
#include <mbedtls/sha256.h>

#include "net.h"
#include "ui.h"

static bool download(const char *expectedSha, size_t size) {
  HTTPClient http;
  WiFiClient *client = nullptr;
  const int status = netOpen(http, client, netUrl("/firmware/bin"));
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  if (!Update.begin(size)) {
    Serial.printf("[ota] begin falhou: %s\n", Update.errorString());
    http.end();
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  WiFiClient *stream = http.getStreamPtr();
  uint8_t buf[1024];
  size_t total = 0;
  unsigned long idleSince = millis();

  while (total < size) {
    const size_t available = stream->available();
    if (available == 0) {
      if (!http.connected() || millis() - idleSince > 15000) break;
      delay(5);
      continue;
    }

    const size_t chunk = stream->readBytes(buf, available > sizeof(buf) ? sizeof(buf) : available);
    if (chunk == 0) continue;

    if (Update.write(buf, chunk) != chunk) {
      Serial.printf("[ota] write falhou: %s\n", Update.errorString());
      break;
    }
    mbedtls_sha256_update(&sha, buf, chunk);
    total += chunk;
    idleSince = millis();

    if ((total % (64 * 1024)) < sizeof(buf)) {
      char line[24];
      snprintf(line, sizeof(line), "%u%%", (unsigned)(total * 100 / size));
      uiStatus(line);
    }
  }

  http.end();

  uint8_t digest[32];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);

  char hex[65];
  for (int i = 0; i < 32; ++i) snprintf(hex + i * 2, 3, "%02x", digest[i]);

  if (total != size || strcasecmp(hex, expectedSha) != 0) {
    Serial.printf("[ota] sha divergente (%u/%u bytes)\n", (unsigned)total, (unsigned)size);
    Update.abort();
    return false;
  }

  if (!Update.end(true)) {
    Serial.printf("[ota] end falhou: %s\n", Update.errorString());
    return false;
  }

  return true;
}

bool otaCheck() {
  if (!netOnline()) return false;

  JsonDocument manifest;
  if (!netGetJson(netUrl("/firmware/version"), manifest)) return false;

  const char *version = manifest["version"] | "";
  const char *sha = manifest["sha256"] | "";
  const size_t size = manifest["size"] | 0;

  if (!version[0] || !sha[0] || size < 1024) return false;
  if (strcmp(version, DASH_FW_VERSION) == 0) return false;

  Serial.printf("[ota] %s -> %s, %u bytes\n", DASH_FW_VERSION, version, (unsigned)size);
  uiSplash("atualizando", "0%");

  if (!download(sha, size)) {
    uiStatus("falhou, seguindo");
    delay(1500);
    uiShowDash();
    return false;
  }

  uiStatus("reiniciando");
  delay(600);
  ESP.restart();
  return true;
}
