#pragma once

#include <Arduino.h>

#include "display.h"
#include "net.h"

void uiBegin();
void uiTask();

void uiSplash(const char *line1, const char *line2);
void uiShowProvisioning(bool armed);
void uiShowDash();

void uiRebuild();
void uiRefreshValues();

void uiTurnPage(int delta);
int uiPage();

void uiMenuToggle();
bool uiMenuOpen();
void uiMenuMove(int delta);
void uiMenuSelect();

void uiSetNight(bool on);
bool uiIsNight();

void uiBusy(bool on);
void uiSuspend();
void uiResume();
void uiStatus(const char *text);
