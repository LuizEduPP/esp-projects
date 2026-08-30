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
const char *netProvName();
void netForgetCredentials();
const char *netSsid();
int netRssiBars();
const char *netIp();

bool netTimeReady();
void netSyncTime();
void netApplyTimezone(long offsetSec);

const char *netUrl(const char *path);

int netOpen(HTTPClient &http, WiFiClient *&client, const char *url);

bool netGetJson(const char *url, JsonDocument &out);

bool netGetToFile(const char *url, const char *path);

bool netFetchDash();
