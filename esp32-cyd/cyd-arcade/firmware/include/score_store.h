#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "ui_keyboard.h"

int score_store_get(const char* engine);
bool score_store_save(const char* engine, int score);
void score_store_get_name(const char* engine, char* out, size_t out_len);
void score_store_set_name(const char* engine, const char* name);
