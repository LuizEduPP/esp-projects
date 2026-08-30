#include "net.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiProv.h>
#include <esp_bt.h>

#include <time.h>

#include "data.h"

static Preferences sPrefs;
static NetState sState = NET_OFFLINE;
static bool sTimeReady = false;
static unsigned long sNextRetry = 0;
static unsigned long sProvisionUntil = 0;
static String sSsid;
static String sPass;
static char sIp[16] = "0.0.0.0";
static char sProvName[16] = "";
static bool sBtFreed = false;
static unsigned long sOfflineSince = 0;
static unsigned long sRetryStored = 0;

static void loadCredentials() {
  if (sPrefs.begin("dash", false)) sPrefs.end();
  sPrefs.begin("dash", true);
  sSsid = sPrefs.getString("ssid", WIFI_SSID);
  sPass = sPrefs.getString("pass", WIFI_PASS);
  sPrefs.end();
}

static void saveCredentials(const String &ssid, const String &pass) {
  sPrefs.begin("dash", false);
  sPrefs.putString("ssid", ssid);
  sPrefs.putString("pass", pass);
  sPrefs.end();
}

static void connectStored() {
  if (sSsid.isEmpty()) {
    netStartProvisioning();
    return;
  }
  Serial.printf("[wifi] connecting to %s\n", sSsid.c_str());
  WiFi.begin(sSsid.c_str(), sPass.c_str());
  sState = NET_CONNECTING;
  sNextRetry = millis() + DASH_WIFI_RETRY_MS;
  if (sOfflineSince == 0) sOfflineSince = millis();
}

void netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.setAutoReconnect(true);
  loadCredentials();
  connectStored();
}

NetState netState() { return sState; }

bool netOnline() { return WiFi.status() == WL_CONNECTED; }

const char *netSsid() { return sSsid.isEmpty() ? "--" : sSsid.c_str(); }

const char *netIp() { return sIp; }

int netRssiBars() {
  if (!netOnline()) return 0;
  const int rssi = WiFi.RSSI();
  if (rssi >= -60) return 3;
  if (rssi >= -70) return 2;
  if (rssi >= -80) return 1;
  return 0;
}

const char *netProvName() { return sProvName; }

void netStartProvisioning() {
  const uint32_t id = (uint32_t)(ESP.getEfuseMac() >> 24);
  snprintf(sProvName, sizeof(sProvName), "DASH-%04X", (uint16_t)(id & 0xFFFF));

  Serial.printf("[wifi] provisionamento BLE em %s\n", sProvName);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFiProv.beginProvision(WIFI_PROV_SCHEME_BLE, WIFI_PROV_SCHEME_HANDLER_FREE_BTDM,
                          WIFI_PROV_SECURITY_1, DASH_PROV_POP, sProvName);

  sState = NET_PROVISIONING;
  sProvisionUntil = millis() + DASH_PROVISION_TIMEOUT_MS;
  sRetryStored = millis() + DASH_PROVISION_TIMEOUT_MS;
}

void netForgetCredentials() {
  sPrefs.begin("dash", false);
  sPrefs.remove("ssid");
  sPrefs.remove("pass");
  sPrefs.end();
  sSsid = "";
  sPass = "";

  if (sBtFreed) {
    Serial.println("[wifi] credenciais apagadas, reiniciando para abrir o BLE");
    delay(300);
    ESP.restart();
  }

  netStartProvisioning();
}

void netLoop() {
  if (netOnline()) {
    if (sState == NET_PROVISIONING) {
      saveCredentials(WiFi.SSID(), WiFi.psk());
      Serial.printf("[wifi] provisionado %s, reiniciando\n", WiFi.SSID().c_str());
      delay(600);
      ESP.restart();
    }
    if (sState != NET_ONLINE) {
      sState = NET_ONLINE;
      sOfflineSince = 0;
      snprintf(sIp, sizeof(sIp), "%s", WiFi.localIP().toString().c_str());
      WiFi.scanDelete();
      if (!sBtFreed) {
        sBtFreed = true;
        esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
      }
      Serial.printf("[wifi] online %s, heap %u\n", sIp, (unsigned)ESP.getMaxAllocHeap());
    }
    return;
  }

  if (sState == NET_PROVISIONING) {
    if ((long)(millis() - sRetryStored) >= 0 && !sSsid.isEmpty()) {
      sRetryStored = millis() + DASH_PROVISION_TIMEOUT_MS;
      WiFi.begin(sSsid.c_str(), sPass.c_str());
    }
    return;
  }

  sState = NET_CONNECTING;
  if (sOfflineSince == 0) sOfflineSince = millis();
  if ((long)(millis() - sOfflineSince) >= (long)DASH_OFFLINE_PROV_MS) {
    if (sBtFreed) {
      Serial.println("[wifi] offline demais, reiniciando para liberar o BLE");
      delay(300);
      ESP.restart();
    }
    netStartProvisioning();
    return;
  }

  if ((long)(millis() - sNextRetry) < 0) return;
  sNextRetry = millis() + DASH_WIFI_RETRY_MS;
  WiFi.disconnect();
  connectStored();
}

bool netTimeReady() { return sTimeReady; }

void netApplyTimezone(long offsetSec) {
  const long totalMin = offsetSec / 60;
  const int h = (int)(totalMin / 60);
  const int m = abs((int)(totalMin % 60));
  char name[12];
  snprintf(name, sizeof(name), "%+03d%02d", h, m);
  char tz[24];
  const int posixH = -h;
  if (m) {
    snprintf(tz, sizeof(tz), "<%s>%d:%02d", name, posixH, m);
  } else {
    snprintf(tz, sizeof(tz), "<%s>%d", name, posixH);
  }
  setenv("TZ", tz, 1);
  tzset();
  Serial.printf("[tz] %s\n", tz);
}

void netSyncTime() {
  if (!netOnline()) return;
  configTime(0, 0, DASH_NTP_SERVER);
  struct tm t;
  if (getLocalTime(&t, 5000) && t.tm_year > 120) {
    sTimeReady = true;
    Serial.println("[ntp] synced");
  }
}

const char *netUrl(const char *path) {
  static char url[160];
  snprintf(url, sizeof(url), "%s%s", DASH_API_URL, path ? path : "");
  return url;
}

static WiFiClientSecure sTls;
static WiFiClient sPlain;

int netOpen(HTTPClient &http, WiFiClient *&client, const char *url) {
  if (!netOnline()) return -1;

  const bool secure = strncmp(url, "https", 5) == 0;
  if (secure) {
    sTls.setInsecure();
    client = &sTls;
  } else {
    client = &sPlain;
  }

  http.setTimeout(DASH_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);
  if (!http.begin(*client, url)) return -1;

  const int status = http.GET();
  if (status <= 0) {
    Serial.printf("[net] %s falhou (%d) heap %u maior bloco %u\n", url, status,
                  (unsigned)ESP.getFreeHeap(), (unsigned)ESP.getMaxAllocHeap());
    if (secure) sTls.stop();
  }
  return status;
}

bool netGetJson(const char *url, JsonDocument &out) {
  HTTPClient http;
  WiFiClient *client = nullptr;
  const int status = netOpen(http, client, url);
  if (status != HTTP_CODE_OK) {
    if (status > 0) Serial.printf("[net] %s HTTP %d\n", url, status);
    http.end();
    return false;
  }

  const DeserializationError err = deserializeJson(out, http.getStream());
  http.end();
  if (err) {
    Serial.printf("[net] %s json: %s\n", url, err.c_str());
    return false;
  }
  return true;
}

bool netGetToFile(const char *url, const char *path) {
  HTTPClient http;
  WiFiClient *client = nullptr;
  const int status = netOpen(http, client, url);
  if (status != HTTP_CODE_OK) {
    http.end();
    return false;
  }

  File file = LittleFS.open(path, "w");
  if (!file) {
    http.end();
    return false;
  }

  const int written = http.writeToStream(&file);
  file.close();
  http.end();

  if (written <= 0) {
    LittleFS.remove(path);
    Serial.printf("[net] %s falhou ao gravar\n", url);
    return false;
  }
  return true;
}

bool netFetchDash() {
  JsonDocument fresh;
  if (!netGetJson(netUrl("/dash"), fresh)) return false;

  JsonDocument &doc = dataDoc();
  for (JsonPairConst kv : fresh.as<JsonObjectConst>()) doc[kv.key()] = kv.value();

  dataMarkValid(true);
  dataUpdateSun();

  const long tz = doc["tz"] | 0;
  if (tz != 0) netApplyTimezone(tz);

  Serial.printf("[dash] %s, %.0fC, heap %u\n", doc["city"] | "--",
                doc["weather"]["temp"] | 0.0f, (unsigned)ESP.getFreeHeap());
  return true;
}
