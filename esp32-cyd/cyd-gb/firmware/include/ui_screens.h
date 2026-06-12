#pragma once
#include <stdint.h>

void ui_draw_splash(int progress_pct);
void ui_animate_splash(uint32_t duration_ms);
void ui_draw_sd_error();
void ui_draw_loading(const char* title, int progress_pct);
bool ui_loading_run(const char* title, bool (*load_fn)(void));
void ui_draw_no_roms_empty();
void ui_show_toast(const char* msg, uint16_t color);
