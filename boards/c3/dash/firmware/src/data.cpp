#include "data.h"

#include <WiFi.h>
#include <string.h>
#include <time.h>

#include "net.h"

static JsonDocument sDoc;
static bool sValid = false;

static const char *const kWeekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
static const char *const kMonths[] = {"janeiro",   "fevereiro", "marco",    "abril",
                                      "maio",      "junho",     "julho",    "agosto",
                                      "setembro",  "outubro",   "novembro", "dezembro"};

JsonDocument &dataDoc() { return sDoc; }

bool dataValid() { return sValid; }

void dataMarkValid(bool valid) { sValid = valid; }

void dataUpdateClock(bool timeReady) {
  struct tm now;
  if (!timeReady || !getLocalTime(&now, 20)) {
    sDoc["time"] = "--:--";
    sDoc["weekday"] = "--";
    sDoc["date"] = "sincronizando";
    return;
  }

  char buf[24];
  snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
  sDoc["time"] = buf;
  sDoc["weekday"] = kWeekdays[now.tm_wday % 7];
  snprintf(buf, sizeof(buf), "%d de %s", now.tm_mday, kMonths[now.tm_mon % 12]);
  sDoc["date"] = buf;
}

void dataUpdateSystem() {
  JsonObject net = sDoc["net"].to<JsonObject>();
  net["ssid"] = netSsid();
  net["ip"] = netIp();
  net["rssi"] = netOnline() ? (int)WiFi.RSSI() : 0;

  JsonObject sys = sDoc["sys"].to<JsonObject>();
  sys["heap"] = (int)(ESP.getFreeHeap() / 1024);

  const unsigned long up = millis() / 1000;
  char buf[16];
  snprintf(buf, sizeof(buf), "%luh %02lum", up / 3600, (up % 3600) / 60);
  sys["uptime"] = buf;
}

void dataUpdateSun() {
  JsonObject sun = sDoc["sun"].to<JsonObject>();
  const char *rise = sDoc["weather"]["sunrise"] | "--:--";
  const char *set = sDoc["weather"]["sunset"] | "--:--";

  int rh = 0, rm = 0, sh = 0, sm = 0;
  if (sscanf(rise, "%d:%d", &rh, &rm) != 2 || sscanf(set, "%d:%d", &sh, &sm) != 2) {
    sun["daylight"] = "--";
    sun["progress"] = 0;
    return;
  }

  const int riseMin = rh * 60 + rm;
  const int setMin = sh * 60 + sm;
  const int span = setMin - riseMin;

  char buf[16];
  snprintf(buf, sizeof(buf), "%dh %02dmin", span / 60, span % 60);
  sun["daylight"] = buf;

  int progress = 0;
  struct tm now;
  if (span > 0 && getLocalTime(&now, 20)) {
    progress = (now.tm_hour * 60 + now.tm_min - riseMin) * 100 / span;
    if (progress < 0) progress = 0;
    if (progress > 100) progress = 100;
  }
  sun["progress"] = progress;
}

JsonVariantConst dataAt(const char *path) {
  JsonVariantConst node = sDoc.as<JsonVariantConst>();
  if (!path || !path[0]) return node;

  char key[32];
  const char *cursor = path;
  while (*cursor && !node.isNull()) {
    const char *dot = strchr(cursor, '.');
    const size_t len = dot ? (size_t)(dot - cursor) : strlen(cursor);
    if (len >= sizeof(key)) return JsonVariantConst();
    memcpy(key, cursor, len);
    key[len] = '\0';

    if (key[0] >= '0' && key[0] <= '9') {
      node = node[(size_t)atoi(key)];
    } else {
      node = node[key];
    }

    cursor = dot ? dot + 1 : cursor + len;
  }
  return node;
}

void dataFormat(const char *path, const char *fmt, char *out, size_t len) {
  JsonVariantConst value = dataAt(path);

  if (value.isNull()) {
    snprintf(out, len, "--");
    return;
  }

  if (!fmt || !fmt[0]) {
    if (value.is<const char *>()) {
      snprintf(out, len, "%s", value.as<const char *>());
    } else if (value.is<float>()) {
      snprintf(out, len, "%.1f", value.as<float>());
    } else {
      snprintf(out, len, "%d", value.as<int>());
    }
    return;
  }

  const char *spec = strchr(fmt, '%');
  while (spec && spec[1] == '%') spec = strchr(spec + 2, '%');
  if (!spec) {
    snprintf(out, len, "%s", fmt);
    return;
  }

  char conv = 0;
  for (const char *c = spec + 1; *c; ++c) {
    if (*c == 'd' || *c == 'f' || *c == 's') {
      conv = *c;
      break;
    }
  }

  switch (conv) {
    case 'd':
      snprintf(out, len, fmt, value.as<int>());
      break;
    case 'f':
      snprintf(out, len, fmt, value.as<float>());
      break;
    case 's':
      snprintf(out, len, fmt, value.as<const char *>() ? value.as<const char *>() : "--");
      break;
    default:
      snprintf(out, len, "%s", fmt);
      break;
  }
}
