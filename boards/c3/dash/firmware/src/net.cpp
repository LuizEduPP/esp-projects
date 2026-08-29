#include "net.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include <time.h>

static bool sTimeReady = false;
static unsigned long sNextRetry = 0;

void netBegin() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  sNextRetry = millis() + DASH_WIFI_RETRY_MS;
}

bool netOnline() { return WiFi.status() == WL_CONNECTED; }

bool netEnsure() {
  if (netOnline()) return true;
  if (millis() < sNextRetry) return false;
  sNextRetry = millis() + DASH_WIFI_RETRY_MS;
  Serial.println("[wifi] reconnecting");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  return false;
}

bool netTimeReady() { return sTimeReady; }

void netSyncTime() {
  if (!netOnline()) return;
  configTzTime(DASH_TZ, DASH_NTP_SERVER);
  struct tm t;
  if (getLocalTime(&t, 5000) && t.tm_year > 120) {
    sTimeReady = true;
    Serial.println("[ntp] synced");
  }
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
    case 99: return "tempestade c/ granizo";
    default: return "--";
  }
}

bool netFetchWeather(Weather &out) {
  if (!netOnline()) return false;

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(DASH_HTTP_TIMEOUT_MS / 1000);

  String url = "https://api.open-meteo.com/v1/forecast?latitude=";
  url += DASH_LAT;
  url += "&longitude=";
  url += DASH_LON;
  url += "&current=temperature_2m,relative_humidity_2m,weather_code,wind_speed_10m";

  HTTPClient http;
  http.setTimeout(DASH_HTTP_TIMEOUT_MS);
  if (!http.begin(client, url)) return false;

  const int status = http.GET();
  if (status != HTTP_CODE_OK) {
    Serial.printf("[weather] HTTP %d\n", status);
    http.end();
    return false;
  }

  JsonDocument filter;
  filter["current"]["temperature_2m"] = true;
  filter["current"]["relative_humidity_2m"] = true;
  filter["current"]["weather_code"] = true;
  filter["current"]["wind_speed_10m"] = true;

  JsonDocument doc;
  const DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("[weather] json: %s\n", err.c_str());
    return false;
  }

  JsonObject cur = doc["current"];
  if (cur.isNull()) return false;

  out.tempC = cur["temperature_2m"] | 0.0f;
  out.humidity = cur["relative_humidity_2m"] | 0;
  out.windKph = cur["wind_speed_10m"] | 0.0f;
  out.code = cur["weather_code"] | -1;
  out.desc = describeCode(out.code);
  out.valid = true;
  return true;
}

bool netFetchInsight(const Weather &w, char *out, size_t outLen) {
  if (!netOnline() || outLen == 0) return false;

  struct tm now;
  if (!getLocalTime(&now, 100)) return false;

  char user[192];
  if (w.valid) {
    snprintf(user, sizeof(user),
             "Sao %02d:%02d. Tempo em %s: %s, %.0f graus, umidade %d%%.", now.tm_hour,
             now.tm_min, DASH_CITY, w.desc, w.tempC, w.humidity);
  } else {
    snprintf(user, sizeof(user), "Sao %02d:%02d em %s. Sem dados de clima.", now.tm_hour,
             now.tm_min, DASH_CITY);
  }

  JsonDocument req;
  req["model"] = DASH_OLLAMA_MODEL;
  req["stream"] = false;
  JsonObject opts = req["options"].to<JsonObject>();
  opts["temperature"] = 0.8;
  opts["num_predict"] = 60;
  JsonArray msgs = req["messages"].to<JsonArray>();
  JsonObject sys = msgs.add<JsonObject>();
  sys["role"] = "system";
  sys["content"] =
      "Voce e o painel de um relogio de mesa. Responda em portugues do Brasil, "
      "no maximo 20 palavras, uma frase util ou espirituosa sobre a hora e o "
      "clima. Sem emojis, sem acentos, sem aspas.";
  JsonObject usr = msgs.add<JsonObject>();
  usr["role"] = "user";
  usr["content"] = user;

  String body;
  serializeJson(req, body);

  WiFiClient client;
  HTTPClient http;
  http.setTimeout(DASH_AI_TIMEOUT_MS);
  if (!http.begin(client, DASH_OLLAMA_URL)) return false;
  http.addHeader("Content-Type", "application/json");

  const int status = http.POST(body);
  if (status != HTTP_CODE_OK) {
    Serial.printf("[ai] HTTP %d\n", status);
    http.end();
    return false;
  }

  JsonDocument filter;
  filter["message"]["content"] = true;

  JsonDocument res;
  const DeserializationError err =
      deserializeJson(res, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("[ai] json: %s\n", err.c_str());
    return false;
  }

  const char *content = res["message"]["content"];
  if (!content || !*content) return false;

  snprintf(out, outLen, "%s", content);
  return true;
}
