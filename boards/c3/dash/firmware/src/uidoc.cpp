#include "uidoc.h"

#include <LittleFS.h>

#include "net.h"

#define META_PATH "/ui/meta.json"

struct PageRef {
  char id[16];
  char title[20];
};

static PageRef sPages[UIDOC_MAX_PAGES];
static int sCount = 0;
static int sVersion = 0;

static void pagePath(const char *id, char *out, size_t len) {
  snprintf(out, len, "/ui/%s.json", id);
}

static void loadMeta() {
  sCount = 0;
  sVersion = 0;

  File file = LittleFS.open(META_PATH, "r");
  if (!file) return;

  JsonDocument meta;
  const DeserializationError err = deserializeJson(meta, file);
  file.close();
  if (err) return;

  sVersion = meta["version"] | 0;
  for (JsonObjectConst page : meta["pages"].as<JsonArrayConst>()) {
    if (sCount >= UIDOC_MAX_PAGES) break;
    snprintf(sPages[sCount].id, sizeof(sPages[0].id), "%s", page["id"] | "");
    snprintf(sPages[sCount].title, sizeof(sPages[0].title), "%s", page["title"] | "--");
    ++sCount;
  }
}

void uiDocBegin() {
  if (!LittleFS.begin(true)) {
    Serial.println("[ui] LittleFS falhou");
    return;
  }
  LittleFS.mkdir("/ui");
  loadMeta();
  Serial.printf("[ui] cache versao %d, %d telas\n", sVersion, sCount);
}

bool uiDocReady() { return sCount > 0; }

int uiDocVersion() { return sVersion; }

int uiDocPageCount() { return sCount; }

const char *uiDocPageId(int index) {
  return (index >= 0 && index < sCount) ? sPages[index].id : "--";
}

const char *uiDocPageTitle(int index) {
  return (index >= 0 && index < sCount) ? sPages[index].title : "--";
}

bool uiDocLoadPage(int index, JsonDocument &out) {
  if (index < 0 || index >= sCount) return false;

  char path[32];
  pagePath(sPages[index].id, path, sizeof(path));
  File file = LittleFS.open(path, "r");
  if (!file) return false;

  const DeserializationError err = deserializeJson(out, file);
  file.close();
  return !err;
}

bool uiDocSync(bool force) {
  if (!netOnline()) return false;

  JsonDocument meta;
  if (!netGetJson(netUrl("/ui/version"), meta)) return false;

  const int remote = meta["version"] | 0;
  if (!force && remote == sVersion && sCount > 0) return false;

  Serial.printf("[ui] baixando versao %d\n", remote);

  int downloaded = 0;
  char url[96];
  char path[32];

  for (JsonObjectConst page : meta["pages"].as<JsonArrayConst>()) {
    if (downloaded >= UIDOC_MAX_PAGES) break;
    const char *id = page["id"] | "";
    if (!id[0]) continue;

    snprintf(url, sizeof(url), "%s/ui/page/%s", DASH_API_URL, id);
    pagePath(id, path, sizeof(path));
    if (!netGetToFile(url, path)) {
      Serial.printf("[ui] tela %s falhou, mantendo cache\n", id);
      return false;
    }
    ++downloaded;
  }

  if (downloaded == 0) return false;

  File file = LittleFS.open(META_PATH, "w");
  if (!file) return false;
  serializeJson(meta, file);
  file.close();

  loadMeta();
  Serial.printf("[ui] versao %d gravada, %d telas\n", sVersion, sCount);
  return true;
}
