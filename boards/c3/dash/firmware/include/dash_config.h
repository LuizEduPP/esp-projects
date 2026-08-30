#pragma once

#ifndef WIFI_SSID
#define WIFI_SSID ""
#endif
#ifndef WIFI_PASS
#define WIFI_PASS ""
#endif

// Base da API. Os caminhos (/dash, /ui, /firmware) sao anexados no netUrl().
#ifndef DASH_API_URL
#define DASH_API_URL "https://dash-esp32.kmali.online"
#endif

#ifndef DASH_NTP_SERVER
#define DASH_NTP_SERVER "pool.ntp.org"
#endif

#ifndef DASH_API_INTERVAL_MS
#define DASH_API_INTERVAL_MS 300000UL
#endif
#ifndef DASH_UI_POLL_MS
#define DASH_UI_POLL_MS 10000UL
#endif
#ifndef DASH_OTA_INTERVAL_MS
#define DASH_OTA_INTERVAL_MS 3600000UL
#endif
#ifndef DASH_FW_VERSION
#define DASH_FW_VERSION "dev"
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
#define DASH_HTTP_TIMEOUT_MS 12000
#endif
#ifndef DASH_PROVISION_TIMEOUT_MS
#define DASH_PROVISION_TIMEOUT_MS 180000UL
#endif

#ifndef DASH_POMODORO_WORK_S
#define DASH_POMODORO_WORK_S 1500
#endif
#ifndef DASH_POMODORO_BREAK_S
#define DASH_POMODORO_BREAK_S 300
#endif
