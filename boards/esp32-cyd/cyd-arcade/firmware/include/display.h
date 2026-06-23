#pragma once
#include <TFT_eSPI.h>
#include <stdbool.h>
#include <stdint.h>

extern TFT_eSPI tft;

void display_init();
void display_splash(const char* title);
bool display_splash_wait(uint32_t duration_ms);

void display_brightness_init();
uint8_t display_get_brightness();
void display_set_brightness(uint8_t level);
int display_brightness_percent();
void display_brightness_step(int delta);
void display_brightness_refresh();
