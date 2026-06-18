#pragma once

#include <Arduino.h>

void motorBegin();
void motorSet(int left, int right);
void motorPoll();
void motorAfterCapture();
