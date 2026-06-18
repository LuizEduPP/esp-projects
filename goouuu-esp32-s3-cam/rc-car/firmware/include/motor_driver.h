#pragma once

#include <Arduino.h>

void motorBegin();
void motorSet(int left, int right);
void motorSetTagged(int left, int right, const char *tag);
void motorPoll();

String motorDiagJson();
void motorRunSelfTest();
