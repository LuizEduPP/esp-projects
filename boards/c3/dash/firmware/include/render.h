#pragma once

#include <ArduinoJson.h>
#include <lvgl.h>

// Constroi os objetos LVGL de uma tela descrita pelo documento de UI.
// So existe uma tela viva por vez: renderClear apaga tudo antes de montar a proxima.
void renderPage(lv_obj_t *parent, JsonObjectConst page);
void renderClear();

// Reaplica os valores nos itens com bind, sem reconstruir a arvore.
void renderRefresh();
