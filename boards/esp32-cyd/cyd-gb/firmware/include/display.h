#pragma once
#include <TFT_eSPI.h>
extern TFT_eSPI tft;

void display_init();
void display_set_backlight(uint8_t level);
uint8_t display_get_backlight();
void display_clear(uint16_t color = TFT_BLACK);
void display_push_gb_line(uint8_t line_y, uint16_t* buf160);
void display_draw_game_frame();
void display_draw_status_bar(const char* title, uint32_t fps);
void display_update_status_fps(uint32_t fps);
void display_draw_controls();
void display_update_dpad(uint8_t dirs, int16_t stick_dx, int16_t stick_dy);
void display_reset_dpad(void);
void display_draw_pause_btn();
