#include "i18n.h"

static uint8_t cur_lang = LANG_EN;

static const char* const T[LANG_COUNT][STR_COUNT] = {
    {
        "Jogos", "%d ROMs", "Nenhuma ROM!", "/roms/gb/*.gb", "coloque .gb no SD",
        "< Ant", "Prox >", "Pausado", "Continuar", "Salvar", "Carregar",
        "Ajustes", "Calibrar", "Sair", "Ajustes", "Paleta", "Frame skip",
        "Brilho", "Idioma", "Salvar e voltar", "SALVO!", "CARREGADO!",
        "Carregando...", "Falha ao abrir!", "Falha ao iniciar!", "Erro no SD!",
        "Insira SD FAT32", "reinicie o aparelho", "Game Boy", "iniciando...",
        "lendo ROM...", "fecha em 1.2s", "fecha em 2s", "CALIBRACAO",
        "Toque em cada + com cuidado", "Superior esq.", "Superior dir.",
        "Inferior esq.", "Inferior dir.", "Centro", "%d/5: %s",
        "Passo %d de 5", "Aguarde 3s entre pontos",
        "Calibracao salva!", "Calibracao falhou!", "Usando mapa padrao",
        "Portugues", "English", "Espanol",
    },
    {
        "Games", "%d ROMs", "No ROMs found!", "/roms/gb/*.gb", "add .gb files to SD",
        "< Prev", "Next >", "Paused", "Resume", "Save", "Load",
        "Settings", "Calibrate", "Quit", "Settings", "Palette", "Frame skip",
        "Brightness", "Language", "Save and back", "SAVED!", "LOADED!",
        "Loading...", "Open failed!", "Init failed!", "SD card error!",
        "Insert FAT32 SD", "reset device", "Game Boy", "boot...",
        "reading ROM...", "auto close 1.2s", "auto close 2s", "CALIBRATION",
        "Touch each + carefully", "Top-Left", "Top-Right",
        "Bottom-Left", "Bottom-Right", "Center", "%d/5: %s",
        "Step %d of 5", "Hold 3s between points",
        "Calibration saved!", "Calibration failed!", "Using factory map",
        "Portuguese", "English", "Spanish",
    },
    {
        "Juegos", "%d ROMs", "Sin ROMs!", "/roms/gb/*.gb", "agrega .gb al SD",
        "< Ant", "Sig >", "Pausado", "Continuar", "Guardar", "Cargar",
        "Ajustes", "Calibrar", "Salir", "Ajustes", "Paleta", "Frame skip",
        "Brillo", "Idioma", "Guardar y volver", "GUARDADO!", "CARGADO!",
        "Cargando...", "Error al abrir!", "Error al iniciar!", "Error de SD!",
        "Inserta SD FAT32", "reinicia el equipo", "Game Boy", "iniciando...",
        "leyendo ROM...", "cierra en 1.2s", "cierra en 2s", "CALIBRACION",
        "Toca cada + con cuidado", "Arriba izq.", "Arriba der.",
        "Abajo izq.", "Abajo der.", "Centro", "%d/5: %s",
        "Paso %d de 5", "Espera 3s entre puntos",
        "Calibracion guardada!", "Calibracion fallo!", "Mapa de fabrica",
        "Portugues", "English", "Espanol",
    },
};

void i18n_set_lang(uint8_t lang) {
    if (lang >= LANG_COUNT) lang = LANG_EN;
    cur_lang = lang;
}

uint8_t i18n_get_lang() {
    return cur_lang;
}

const char* tr(StringId id) {
    if (id >= STR_COUNT) return "?";
    return T[cur_lang][id];
}

const char* i18n_lang_label(uint8_t lang) {
    if (lang >= LANG_COUNT) lang = LANG_EN;
    static const StringId ids[LANG_COUNT] = {STR_LANG_PT, STR_LANG_EN, STR_LANG_ES};
    return tr(ids[lang]);
}
