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

static bool httpGetJson(const char *url, JsonDocument &doc) {
  WiFiClient client;
  HTTPClient http;
  http.setTimeout(DASH_HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) return false;

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[http] %s -> %d\n", url, status);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();
  return !deserializeJson(doc, payload);
}

static bool geocodeCity(Place &out) {
  if (!strlen(DASH_CITY)) return false;

  String url = DASH_GEOCODE_URL;
  for (const char *p = DASH_CITY; *p; ++p) url += (*p == ' ') ? '+' : *p;

  JsonDocument doc;
  if (!httpGetJson(url.c_str(), doc)) return false;

  JsonObject hit = doc["results"][0];
  if (hit.isNull()) return false;

  out.lat = hit["latitude"] | out.lat;
  out.lon = hit["longitude"] | out.lon;
  snprintf(out.city, sizeof(out.city), "%s", DASH_CITY);
  out.valid = true;
  return true;
}

static bool geolocateByIp(Place &out) {
  JsonDocument doc;
  if (!httpGetJson(DASH_GEO_URL, doc)) return false;
  if (strcmp(doc["status"] | "fail", "success") != 0) return false;

  out.lat = doc["lat"] | out.lat;
  out.lon = doc["lon"] | out.lon;
  snprintf(out.city, sizeof(out.city), "%s", doc["city"] | out.city);
  out.valid = true;
  return true;
}

bool netResolvePlace(Place &out) {
  if (!netOnline()) return false;
  if (!geocodeCity(out) && !geolocateByIp(out)) return false;
  Serial.printf("[geo] %s %.4f %.4f\n", out.city, out.lat, out.lon);
  return true;
}

static const char *describeCode(int code) {
  switch (code) {
    case 0: return "ceu limpo";
    case 1: return "predom. limpo";
    case 2: return "parc. nublado";
    case 3: return "encoberto";
    case 45:
    case 48: return "nevoeiro";
    case 51:
    case 53:
    case 55: return "garoa";
    case 56:
    case 57: return "garoa gelada";
    case 61: return "chuva fraca";
    case 63: return "chuva";
    case 65: return "chuva forte";
    case 66:
    case 67: return "chuva gelada";
    case 71:
    case 73:
    case 75:
    case 77: return "neve";
    case 80: return "pancadas fracas";
    case 81: return "pancadas";
    case 82: return "pancadas fortes";
    case 85:
    case 86: return "pancadas de neve";
    case 95: return "tempestade";
    case 96:
    case 99: return "tempestade granizo";
    default: return "--";
  }
}

bool netFetchWeather(Place &place, Weather &out) {
  if (!netOnline()) return false;

  WiFiClient client;

  char url[512];
  snprintf(url, sizeof(url),
           "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f"
           "&current=temperature_2m,relative_humidity_2m,apparent_temperature,"
           "weather_code,wind_speed_10m"
           "&daily=temperature_2m_max,temperature_2m_min,precipitation_probability_max,"
           "weather_code,sunrise,sunset"
           "&hourly=temperature_2m&forecast_days=4&timezone=auto",
           place.lat, place.lon);

  HTTPClient http;
  http.setTimeout(DASH_HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) return false;

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[weather] HTTP %d\n", status);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument doc;
  const DeserializationError err = deserializeJson(doc, payload);
  if (err) {
    Serial.printf("[weather] json: %s\n", err.c_str());
    return false;
  }

  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;

  out.tempC = cur["temperature_2m"] | 0.0f;
  out.feelsC = cur["apparent_temperature"] | out.tempC;
  out.humidity = cur["relative_humidity_2m"] | 0;
  out.windKph = cur["wind_speed_10m"] | 0.0f;
  out.code = cur["weather_code"] | -1;
  out.desc = describeCode(out.code);

  JsonObject daily = doc["daily"];
  if (!daily.isNull()) {
    out.maxC = daily["temperature_2m_max"][0] | out.tempC;
    out.minC = daily["temperature_2m_min"][0] | out.tempC;
    out.rainProb = daily["precipitation_probability_max"][0] | 0;

    const char *rise = daily["sunrise"][0] | "";
    const char *set = daily["sunset"][0] | "";
    if (strlen(rise) >= 16) snprintf(out.sunrise, sizeof(out.sunrise), "%.5s", rise + 11);
    if (strlen(set) >= 16) snprintf(out.sunset, sizeof(out.sunset), "%.5s", set + 11);

    struct tm today;
    const bool haveDate = getLocalTime(&today, 20);
    out.dayCount = 0;
    for (int i = 1; i <= 3; ++i) {
      JsonVariant hi = daily["temperature_2m_max"][i];
      if (hi.isNull()) break;
      Forecast &f = out.days[out.dayCount];
      f.maxC = hi | 0.0f;
      f.minC = daily["temperature_2m_min"][i] | 0.0f;
      f.code = daily["weather_code"][i] | -1;
      f.weekday = haveDate ? (today.tm_wday + i) % 7 : -1;
      ++out.dayCount;
    }
  }

  JsonArray hours = doc["hourly"]["temperature_2m"];
  if (!hours.isNull()) {
    struct tm now;
    out.hourNow = getLocalTime(&now, 20) ? now.tm_hour : 0;
    out.hourlyCount = 0;
    for (int i = 0; i < 24 && i < (int)hours.size(); ++i) {
      out.hourly[out.hourlyCount++] = hours[i] | 0.0f;
    }
  }

  const long offset = doc["utc_offset_seconds"] | 0;
  if (offset != place.tzOffsetSec) {
    place.tzOffsetSec = offset;
    netApplyTimezone(offset);
  }

  out.valid = true;
  Serial.printf("[weather] %s %.1fC (%.0f-%.0f) %d%%\n", out.desc, out.tempC, out.minC,
                out.maxC, out.humidity);
  return true;
}

static char foldLatin1(uint8_t c) {
  if (c >= 0xC0 && c <= 0xC5) return 'A';
  if (c == 0xC7) return 'C';
  if (c >= 0xC8 && c <= 0xCB) return 'E';
  if (c >= 0xCC && c <= 0xCF) return 'I';
  if (c == 0xD1) return 'N';
  if ((c >= 0xD2 && c <= 0xD6) || c == 0xD8) return 'O';
  if (c >= 0xD9 && c <= 0xDC) return 'U';
  if (c == 0xDD) return 'Y';
  if (c >= 0xE0 && c <= 0xE5) return 'a';
  if (c == 0xE7) return 'c';
  if (c >= 0xE8 && c <= 0xEB) return 'e';
  if (c >= 0xEC && c <= 0xEF) return 'i';
  if (c == 0xF1) return 'n';
  if ((c >= 0xF2 && c <= 0xF6) || c == 0xF8) return 'o';
  if (c >= 0xF9 && c <= 0xFC) return 'u';
  if (c == 0xFD || c == 0xFF) return 'y';
  return 0;
}

static void deaccent(const char *in, char *out, size_t outLen) {
  size_t o = 0;
  for (size_t i = 0; in[i] && o + 1 < outLen;) {
    const uint8_t c = (uint8_t)in[i];
    if (c < 0x80) {
      out[o++] = (c == '\n' || c == '\r' || c == '\t') ? ' ' : in[i];
      ++i;
      continue;
    }
    if ((c & 0xE0) == 0xC0 && in[i + 1]) {
      const uint8_t cp = ((c & 0x1F) << 6) | ((uint8_t)in[i + 1] & 0x3F);
      const char folded = foldLatin1(cp);
      if (folded) out[o++] = folded;
      i += 2;
      continue;
    }
    if ((c & 0xF0) == 0xE0) {
      i += 3;
      continue;
    }
    if ((c & 0xF8) == 0xF0) {
      i += 4;
      continue;
    }
    ++i;
  }
  out[o] = '\0';
}

bool netFetchInsight(const Place &p, const Weather &w, char *out, size_t outLen) {
  if (!netOnline() || outLen == 0) return false;

  struct tm now;
  if (!getLocalTime(&now, 100)) return false;

  static const char *kInstruction =
      "Voce e o painel de um relogio de mesa. Escreva UMA frase em portugues do "
      "Brasil, no maximo 18 palavras, util ou espirituosa, sobre a hora e o clima. "
      "Sem emojis, sem aspas, sem explicacao.\n\n";

  char user[384];
  if (w.valid) {
    snprintf(user, sizeof(user),
             "%sSao %02d:%02d em %s. Tempo: %s, %.0f graus, sensacao %.0f, minima "
             "%.0f, maxima %.0f, umidade %d por cento, chance de chuva %d por cento.",
             kInstruction, now.tm_hour, now.tm_min, p.city, w.desc, w.tempC, w.feelsC,
             w.minC, w.maxC, w.humidity, w.rainProb);
  } else {
    snprintf(user, sizeof(user), "%sSao %02d:%02d em %s. Sem dados de clima.",
             kInstruction, now.tm_hour, now.tm_min, p.city);
  }

  JsonDocument req;
  req["model"] = DASH_OLLAMA_MODEL;
  req["stream"] = false;
  req["think"] = false;
  JsonObject opts = req["options"].to<JsonObject>();
  opts["temperature"] = 0.8;
  opts["num_predict"] = 80;
  JsonArray msgs = req["messages"].to<JsonArray>();
  JsonObject usr = msgs.add<JsonObject>();
  usr["role"] = "user";
  usr["content"] = user;

  String body;
  serializeJson(req, body);

  WiFiClient plain;
  WiFiClientSecure tls;
  WiFiClient *client = &plain;
  if (strncmp(DASH_OLLAMA_URL, "https:", 6) == 0) {
    tls.setInsecure();
    tls.setTimeout(DASH_AI_TIMEOUT_MS / 1000);
    client = &tls;
  }

  HTTPClient http;
  http.setTimeout(DASH_AI_TIMEOUT_MS);
  if (!http.begin(*client, DASH_OLLAMA_URL)) return false;
  http.addHeader("Content-Type", "application/json");

  const int status = http.POST(body);
  if (status != HTTP_CODE_OK) {
    Serial.printf("[ai] HTTP %d\n", status);
    http.end();
    return false;
  }

  const String payload = http.getString();
  http.end();

  JsonDocument res;
  const DeserializationError err = deserializeJson(res, payload);
  if (err) {
    Serial.printf("[ai] json: %s\n", err.c_str());
    return false;
  }

  const char *content = res["message"]["content"];
  if (!content || !*content) return false;

  deaccent(content, out, outLen);
  return true;
}
