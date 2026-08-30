#pragma once

#include <ArduinoJson.h>
#include <lvgl.h>

void renderPage(lv_obj_t *parent, JsonObjectConst page);
void renderClear();

void renderRefresh();
