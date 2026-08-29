#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID "SUA_REDE_WIFI"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "SUA_SENHA"
#endif

#ifndef DASH_CITY
#define DASH_CITY "Sao Paulo"
#endif
#ifndef DASH_LAT
#define DASH_LAT "-23.5505"
#endif
#ifndef DASH_LON
#define DASH_LON "-46.6333"
#endif

#ifndef DASH_TZ
#define DASH_TZ "<-03>3"
#endif

#ifndef DASH_NTP_SERVER
#define DASH_NTP_SERVER "pool.ntp.org"
#endif

#ifndef DASH_OLLAMA_URL
#define DASH_OLLAMA_URL "http://192.168.1.28:11434/api/chat"
#endif
#ifndef DASH_OLLAMA_MODEL
#define DASH_OLLAMA_MODEL "llama3.2:3b"
#endif

#ifndef DASH_WEATHER_INTERVAL_MS
#define DASH_WEATHER_INTERVAL_MS 900000UL
#endif
#ifndef DASH_AI_INTERVAL_MS
#define DASH_AI_INTERVAL_MS 1800000UL
#endif
#ifndef DASH_WIFI_RETRY_MS
#define DASH_WIFI_RETRY_MS 15000UL
#endif
#ifndef DASH_HTTP_TIMEOUT_MS
#define DASH_HTTP_TIMEOUT_MS 8000
#endif
#ifndef DASH_AI_TIMEOUT_MS
#define DASH_AI_TIMEOUT_MS 30000
#endif

#define DASH_AI_TEXT_MAX 192
