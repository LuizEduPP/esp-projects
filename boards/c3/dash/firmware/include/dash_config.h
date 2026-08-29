#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

#ifndef DASH_CITY
#define DASH_CITY "--"
#endif
#ifndef DASH_LAT
#define DASH_LAT -23.5505f
#endif
#ifndef DASH_LON
#define DASH_LON -46.6333f
#endif

#ifndef DASH_NTP_SERVER
#define DASH_NTP_SERVER "pool.ntp.org"
#endif

#ifndef DASH_GEO_URL
#define DASH_GEO_URL "http://ip-api.com/json/?fields=status,city,lat,lon"
#endif

#ifndef DASH_OLLAMA_URL
#define DASH_OLLAMA_URL "https://ollama.kmali.online/api/chat"
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
#ifndef DASH_SCREEN_TIMEOUT_MS
#define DASH_SCREEN_TIMEOUT_MS 60000UL
#endif
#ifndef DASH_TIME_SYNC_INTERVAL_MS
#define DASH_TIME_SYNC_INTERVAL_MS 3600000UL
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
#ifndef DASH_PROVISION_TIMEOUT_MS
#define DASH_PROVISION_TIMEOUT_MS 180000UL
#endif

#define DASH_AI_TEXT_MAX 192
