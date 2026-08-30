#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

#define UIDOC_MAX_PAGES 24

// Documento de UI vindo do servidor, gravado pagina a pagina em LittleFS.
// Sem rede, a placa sobe pelo cache; sem cache, cai no layout embutido.
void uiDocBegin();

bool uiDocReady();
int uiDocVersion();
int uiDocPageCount();
const char *uiDocPageId(int index);
const char *uiDocPageTitle(int index);

// Carrega uma pagina do cache. Retorna false se nao existir.
bool uiDocLoadPage(int index, JsonDocument &out);

// Consulta /ui/version; baixa e regrava so quando a versao muda.
// Retorna true quando o documento mudou e a tela precisa ser reconstruida.
bool uiDocSync(bool force);
