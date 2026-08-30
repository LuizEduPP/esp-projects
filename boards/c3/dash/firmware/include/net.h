#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>
#include <HTTPClient.h>

#include "dash_config.h"

enum NetState { NET_OFFLINE, NET_CONNECTING, NET_PROVISIONING, NET_ONLINE };

void netBegin();
NetState netState();
bool netOnline();
void netLoop();

void netStartProvisioning();
void netForgetCredentials();
const char *netSsid();
int netRssiBars();
const char *netIp();

bool netTimeReady();
void netSyncTime();
void netApplyTimezone(long offsetSec);

// Monta DASH_API_URL + path. Buffer proprio, valido ate a proxima chamada.
const char *netUrl(const char *path);

// Abre uma requisicao GET ja com TLS resolvido. Devolve o codigo HTTP;
// o chamador deve sempre chamar http.end().
int netOpen(HTTPClient &http, WiFiClient *&client, const char *url);

bool netGetJson(const char *url, JsonDocument &out);

// Baixa direto para um arquivo do LittleFS, sem passar por RAM.
bool netGetToFile(const char *url, const char *path);

// Busca o payload de dados e joga dentro do documento global de dados.
bool netFetchDash();
