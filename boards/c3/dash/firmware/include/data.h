#pragma once

#include <ArduinoJson.h>
#include <Arduino.h>

// Documento unico com o payload de /dash mais os campos gerados na placa
// (relogio, rede, heap, timer). Os binds do documento de UI apontam para
// caminhos aqui dentro, entao servidor e placa compartilham o mesmo namespace.
JsonDocument &dataDoc();

bool dataValid();
void dataMarkValid(bool valid);

// Campos locais, recalculados a cada tick.
void dataUpdateClock(bool timeReady);
void dataUpdateSystem();
void dataUpdateTimer(int remainingSec, int totalSec, bool breakMode, bool running);
void dataUpdateSun();

// Resolve "weather.days.0.max" contra o documento.
JsonVariantConst dataAt(const char *path);

// Formata usando o fmt do item ("%.0f", "%d dias", "%s"). Sem fmt vira texto cru.
void dataFormat(const char *path, const char *fmt, char *out, size_t len);
