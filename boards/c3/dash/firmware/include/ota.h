#pragma once

#include <Arduino.h>

// Compara DASH_FW_VERSION com /firmware/version. Se diferir, baixa gravando na
// particao ociosa, confere o sha256 e reinicia. A particao em uso fica intacta
// ate a troca, entao falha de rede no meio do download nao quebra a placa.
bool otaCheck();
