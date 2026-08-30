#include "net.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <time.h>

static Preferences sPrefs;
static NetState sState = NET_OFFLINE;
static bool sTimeReady = false;
static unsigned long sNextRetry = 0;
static unsigned long sProvisionUntil = 0;
static String sSsid;
static String sPass;
static char sIp[16] = "0.0.0.0";

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

void netStartProvisioning() {
  Serial.println("[wifi] smartconfig");
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.beginSmartConfig();
  sState = NET_PROVISIONING;
  sProvisionUntil = millis() + DASH_PROVISION_TIMEOUT_MS;
}

void netForgetCredentials() {
  sPrefs.begin("dash", false);
  sPrefs.remove("ssid");
  sPrefs.remove("pass");
  sPrefs.end();
  sSsid = "";
  sPass = "";
  netStartProvisioning();
}

void netLoop() {
  if (netOnline()) {
    if (sState != NET_ONLINE) {
      sState = NET_ONLINE;
      snprintf(sIp, sizeof(sIp), "%s", WiFi.localIP().toString().c_str());
      Serial.printf("[wifi] online %s\n", sIp);
    }
    return;
  }

  if (sState == NET_PROVISIONING) {
    if (WiFi.smartConfigDone()) {
      sSsid = WiFi.SSID();
      sPass = WiFi.psk();
      saveCredentials(sSsid, sPass);
      WiFi.stopSmartConfig();
      Serial.printf("[wifi] provisioned %s\n", sSsid.c_str());
      connectStored();
    } else if ((long)(millis() - sProvisionUntil) >= 0) {
      WiFi.stopSmartConfig();
      connectStored();
    }
    return;
  }

  sState = NET_CONNECTING;
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

static void copyText(const char *src, char *dst, size_t len) {
  snprintf(dst, len, "%s", src && src[0] ? src : "--");
}

bool netFetchDash(Dash &out) {
  if (!netOnline()) return false;

  const bool secure = strncmp(DASH_API_URL, "https", 5) == 0;
  WiFiClient plain;
  WiFiClientSecure tls;
  if (secure) tls.setInsecure();

  HTTPClient http;
  http.setTimeout(DASH_HTTP_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.useHTTP10(true);

  const bool opened = secure ? http.begin(tls, DASH_API_URL) : http.begin(plain, DASH_API_URL);
  if (!opened) return false;

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[dash] HTTP %d\n", status);
    http.end();
    return false;
  }

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, http.getStream());
  http.end();
  if (err) {
    Serial.printf("[dash] json: %s\n", err.c_str());
    return false;
  }

  out.tzOffsetSec = doc["tz"] | 0;
  copyText(doc["city"] | "--", out.city, sizeof(out.city));

  JsonObject w = doc["weather"];
  Weather &weather = out.weather;
  weather.valid = w["valid"] | false;
  weather.tempC = w["temp"] | 0.0f;
  weather.feelsC = w["feels"] | 0.0f;
  weather.minC = w["min"] | 0.0f;
  weather.maxC = w["max"] | 0.0f;
  weather.humidity = w["hum"] | 0;
  weather.windKph = w["wind"] | 0.0f;
  weather.windDir = w["dir"] | 0;
  weather.gustKph = w["gust"] | 0.0f;
  weather.pressure = w["pres"] | 0.0f;
  weather.pressureDelta = w["dpres"] | 0.0f;
  weather.rainProb = w["rainp"] | 0;
  weather.code = w["code"] | -1;
  copyText(w["desc"] | "--", weather.desc, sizeof(weather.desc));
  weather.uvMax = w["uv"] | 0.0f;
  copyText(w["sunrise"] | "--:--", weather.sunrise, sizeof(weather.sunrise));
  copyText(w["sunset"] | "--:--", weather.sunset, sizeof(weather.sunset));
  weather.hourNow = w["hournow"] | 0;
  weather.rainStartsInMin = w["rainin"] | -1;

  weather.hourlyCount = 0;
  for (float t : w["hourly"].as<JsonArray>()) {
    if (weather.hourlyCount >= 24) break;
    weather.hourly[weather.hourlyCount++] = t;
  }

  weather.rain15Count = 0;
  for (float mm : w["rain15"].as<JsonArray>()) {
    if (weather.rain15Count >= 8) break;
    weather.rain15[weather.rain15Count++] = mm;
  }

  weather.dayCount = 0;
  for (JsonObject d : w["days"].as<JsonArray>()) {
    if (weather.dayCount >= 3) break;
    Forecast &slot = weather.days[weather.dayCount++];
    copyText(d["day"] | "--", slot.day, sizeof(slot.day));
    slot.minC = d["min"] | 0.0f;
    slot.maxC = d["max"] | 0.0f;
    slot.code = d["code"] | -1;
  }

  JsonObject a = doc["air"];
  out.air.valid = a["valid"] | false;
  out.air.aqi = a["aqi"] | 0;
  out.air.pm25 = a["pm25"] | 0.0f;
  out.air.pm10 = a["pm10"] | 0.0f;
  copyText(a["label"] | "--", out.air.label, sizeof(out.air.label));

  JsonObject m = doc["moon"];
  out.moon.illum = m["illum"] | 0.0f;
  out.moon.waxing = m["waxing"] | true;
  copyText(m["name"] | "--", out.moon.name, sizeof(out.moon.name));

  out.news.count = 0;
  for (const char *item : doc["news"].as<JsonArray>()) {
    if (out.news.count >= 3) break;
    copyText(item, out.news.items[out.news.count], sizeof(out.news.items[0]));
    ++out.news.count;
  }
  out.news.valid = out.news.count > 0;

  JsonObject k = doc["market"];
  out.market.valid = k["valid"] | false;
  out.market.usd = k["usd"] | 0.0f;
  out.market.usdPct = k["usdPct"] | 0.0f;
  out.market.eur = k["eur"] | 0.0f;
  out.market.eurPct = k["eurPct"] | 0.0f;
  out.market.btc = k["btc"] | 0.0f;
  out.market.btcPct = k["btcPct"] | 0.0f;

  JsonObject r = doc["rates"];
  out.rates.valid = r["valid"] | false;
  out.rates.selic = r["selic"] | 0.0f;
  out.rates.cdi = r["cdi"] | 0.0f;
  out.rates.ipca = r["ipca"] | 0.0f;

  JsonObject h = doc["holiday"];
  out.holiday.valid = h["valid"] | false;
  copyText(h["name"] | "--", out.holiday.name, sizeof(out.holiday.name));
  copyText(h["date"] | "--", out.holiday.date, sizeof(out.holiday.date));
  out.holiday.daysLeft = h["daysLeft"] | 0;

  JsonObject t = doc["history"];
  out.history.valid = t["valid"] | false;
  out.history.year = t["year"] | 0;
  copyText(t["text"] | "--", out.history.text, sizeof(out.history.text));

  JsonObject s = doc["space"];
  out.space.valid = s["valid"] | false;
  out.space.people = s["people"] | 0;
  out.space.lat = s["lat"] | 0.0f;
  out.space.lon = s["lon"] | 0.0f;

  JsonObject d = doc["dev"];
  out.dev.valid = d["valid"] | false;
  out.dev.today = d["today"] | 0;
  out.dev.week = d["week"] | 0;
  out.dev.activeDays = d["activeDays"] | 0;
  out.dev.repos = d["repos"] | 0;
  out.dev.followers = d["followers"] | 0;
  copyText(d["repo"] | "--", out.dev.repo, sizeof(out.dev.repo));

  copyText(doc["ai"] | "", out.ai, sizeof(out.ai));

  out.valid = true;
  Serial.printf("[dash] %s, %.0fC, %d noticias, %d commits\n", out.city, weather.tempC,
                out.news.count, out.dev.today);
  return true;
}

