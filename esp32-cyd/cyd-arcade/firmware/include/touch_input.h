#pragma once
#include <stdint.h>
#include <stdbool.h>

struct CydTouchCal {
    int16_t x_min, x_max, y_min, y_max;
};

void touch_init();
void touch_poll();
void touch_wait_release();
bool touch_is_pressed();
int16_t touch_get_x();
int16_t touch_get_y();
bool touch_in_play_area();
int16_t touch_play_x();
int16_t touch_play_y();
bool touch_read_raw(int16_t* rx, int16_t* ry);
void touch_set_calibration(CydTouchCal cal);
void touch_clear_calibration();
bool touch_has_saved_calibration();
