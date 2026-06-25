#pragma once

#include <Arduino.h>

struct ConsoleInput {
  bool left = false;
  bool right = false;
  bool up = false;
  bool down = false;
  bool a = false;
  bool leftPressed = false;
  bool rightPressed = false;
  bool upPressed = false;
  bool downPressed = false;
  bool aPressed = false;
  bool leftRepeat = false;
  bool rightRepeat = false;
  bool upRepeat = false;
  bool downRepeat = false;
  bool aRepeat = false;
};

bool consoleInputBegin();
void consoleInputPoll();
const ConsoleInput &consoleInputState();


bool consoleInputWantExit();
bool consoleInputTakeA();
