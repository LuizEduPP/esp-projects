#pragma once

#include <Arduino.h>

void motorBegin();
void motorSet(int left, int right);
void motorSetTagged(int left, int right, const char *tag);
void motorSetHold(bool hold);
void motorPoll();
void motorLogHeartbeat();

String motorDiagJson();
void motorRunSelfTest();
