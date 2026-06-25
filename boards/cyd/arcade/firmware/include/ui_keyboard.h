#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SCORE_NAME_LEN 10

bool ui_keyboard_enter_name(const char* title, int score, char* out, int out_len);
