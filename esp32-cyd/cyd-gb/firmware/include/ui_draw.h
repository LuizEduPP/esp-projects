#pragma once
#include <stdint.h>
#include "i18n.h"
#include "ui_icons.h"

void ui_sync(void);
void ui_clear(void);
void ui_wait_release(void);

void ui_bar_header(int h, UiIcon icon, const char* title, int title_x);
void ui_menu_row(int y, int h, UiIcon icon, const char* label, bool hl, bool danger);
void ui_progress_bar(int x, int y, int w, int h, int pct);
void ui_progress_bar_update(int x, int y, int w, int h, int pct);
/* Pass STR_COUNT for optional sub/hint lines */
void ui_status_body(UiIcon icon, int icon_sz, StringId title, StringId sub, StringId hint);
void ui_status_result(UiIcon icon, StringId title, StringId sub, StringId hint, bool ok);
void ui_toast(const char* msg, uint16_t color);
