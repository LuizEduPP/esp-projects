#pragma once

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

bool consoleOledBegin();
Adafruit_SSD1306 *consoleOledDisplay();
void consoleOledShow(const char *line1, const char *line2, const char *line3, const char *line4);
void consoleOledClear();
void consoleOledFlush();
