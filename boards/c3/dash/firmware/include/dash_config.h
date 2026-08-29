#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

#ifndef DASH_CITY
#define DASH_CITY "Cesario Lange"
#endif
#ifndef DASH_LAT
#define DASH_LAT -23.2267f
#endif
#ifndef DASH_LON
#define DASH_LON -47.9531f
#endif

#ifndef DASH_GEOCODE_URL
#define DASH_GEOCODE_URL "http://geocoding-api.open-meteo.com/v1/search?count=1&language=pt&format=json&name="
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
#define DASH_OLLAMA_MODEL "gemma4:e4b"
#endif

#ifndef DASH_WEATHER_INTERVAL_MS
#define DASH_WEATHER_INTERVAL_MS 600000UL
#endif
#ifndef DASH_AI_INTERVAL_MS
#define DASH_AI_INTERVAL_MS 600000UL
#endif
#ifndef DASH_MARKET_INTERVAL_MS
#define DASH_MARKET_INTERVAL_MS 600000UL
#endif
#ifndef DASH_DEV_INTERVAL_MS
#define DASH_DEV_INTERVAL_MS 600000UL
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

#ifndef DASH_MARKET_URL
#define DASH_MARKET_URL "https://economia.awesomeapi.com.br/last/USD-BRL,EUR-BRL,BTC-BRL"
#endif

#ifndef DASH_GITHUB_USER
#define DASH_GITHUB_USER "luizedupp"
#endif

#ifndef DASH_NEWS_URL
#define DASH_NEWS_URL "https://g1.globo.com/rss/g1/"
#endif
#ifndef DASH_HOLIDAY_URL
#define DASH_HOLIDAY_URL "https://brasilapi.com.br/api/feriados/v1/"
#endif
#ifndef DASH_RATES_URL
#define DASH_RATES_URL "https://brasilapi.com.br/api/taxas/v1"
#endif
#ifndef DASH_HISTORY_URL
#define DASH_HISTORY_URL "https://pt.wikipedia.org/api/rest_v1/feed/onthisday/selected/"
#endif
#ifndef DASH_SPACE_URL
#define DASH_SPACE_URL "http://api.open-notify.org/astros.json"
#endif
#ifndef DASH_ISS_URL
#define DASH_ISS_URL "http://api.open-notify.org/iss-now.json"
#endif

#ifndef DASH_SLOW_INTERVAL_MS
#define DASH_SLOW_INTERVAL_MS 1800000UL
#endif

#ifndef DASH_POMODORO_WORK_S
#define DASH_POMODORO_WORK_S 1500
#endif
#ifndef DASH_POMODORO_BREAK_S
#define DASH_POMODORO_BREAK_S 300
#endif

#define DASH_AI_TEXT_MAX 192
