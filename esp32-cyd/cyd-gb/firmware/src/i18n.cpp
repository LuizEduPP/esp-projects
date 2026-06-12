#include "i18n.h"

static uint8_t cur_lang = LANG_EN;

static const char* const T[LANG_COUNT][STR_COUNT] = {
    {
        "Jogos", "%d ROMs", "Nenhuma ROM!", "Coloque .gb em /roms/gb/",
        "< Ant", "Prox >", "Pausado", "Continuar", "Salvar", "Carregar",
        "Ajustes", "Calibrar", "Sair", "Ajustes", "Paleta", "Frame skip",
        "Brilho", "Idioma", "Salvar e voltar", "SALVO!", "CARREGADO!",
        "Carregando...", "Falha ao abrir!", "Falha ao iniciar!", "Erro no SD!",
        "Insira SD FAT32 e reinicie", "Game Boy", "CALIBRACAO",
        "Toque em cada + com cuidado", "Superior esq.", "Superior dir.",
        "Inferior esq.", "Inferior dir.", "Centro", "%d/5: %s",
        "Calibracao salva!", "Calibracao falhou!", "Usando mapa padrao",
        "Portugues", "English", "Espanol",
    },
    {
        "Games", "%d ROMs", "No ROMs found!", "Put .gb files in /roms/gb/",
        "< Prev", "Next >", "Paused", "Resume", "Save", "Load",
        "Settings", "Calibrate", "Quit", "Settings", "Palette", "Frame skip",
        "Brightness", "Language", "Save and back", "SAVED!", "LOADED!",
        "Loading...", "Open failed!", "Init failed!", "SD card error!",
        "Insert FAT32 SD and reset", "Game Boy", "CALIBRATION",
        "Touch each + carefully", "Top-Left", "Top-Right",
        "Bottom-Left", "Bottom-Right", "Center", "%d/5: %s",
        "Calibration saved!", "Calibration failed!", "Using factory map",
        "Portuguese", "English", "Spanish",
    },
    {
        "Juegos", "%d ROMs", "Sin ROMs!", "Pon .gb en /roms/gb/",
        "< Ant", "Sig >", "Pausado", "Continuar", "Guardar", "Cargar",
        "Ajustes", "Calibrar", "Salir", "Ajustes", "Paleta", "Frame skip",
        "Brillo", "Idioma", "Guardar y volver", "GUARDADO!", "CARGADO!",
        "Cargando...", "Error al abrir!", "Error al iniciar!", "Error de SD!",
        "Inserta SD FAT32 y reinicia", "Game Boy", "CALIBRACION",
        "Toca cada + con cuidado", "Arriba izq.", "Arriba der.",
        "Abajo izq.", "Abajo der.", "Centro", "%d/5: %s",
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
