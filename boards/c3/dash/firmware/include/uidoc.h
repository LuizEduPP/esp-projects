#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

#define UIDOC_MAX_PAGES 24

void uiDocBegin();

bool uiDocReady();
int uiDocVersion();
int uiDocPageCount();
const char *uiDocPageId(int index);
const char *uiDocPageTitle(int index);

bool uiDocLoadPage(int index, JsonDocument &out);

bool uiDocSync(bool force);
