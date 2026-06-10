#pragma once

#include <Adafruit_SSD1306.h>

#include "game_scores.h"

// Inverted HUD y 0-14 | gap y 15 | playfield border y 16..63 | inner y 17..62
constexpr int16_t GAME_UI_HUD_H = 15;
constexpr int16_t GAME_UI_SEP_H = 1;
constexpr int16_t GAME_UI_TOP = 16;
constexpr int16_t GAME_UI_H = 48;
constexpr int16_t GAME_UI_BOTTOM = GAME_UI_TOP + GAME_UI_H - 1;
constexpr int16_t GAME_UI_PLAY_Y = GAME_UI_TOP + 1;
constexpr int16_t GAME_UI_PLAY_H = GAME_UI_H - 2;
constexpr int16_t GAME_UI_PLAY_BOTTOM = GAME_UI_PLAY_Y + GAME_UI_PLAY_H - 1;
constexpr int16_t GAME_UI_TEXT_Y = 4;

constexpr uint8_t G_SNAKE = 0;
constexpr uint8_t G_TETRIS = 1;
constexpr uint8_t G_MEMORY = 2;
constexpr uint8_t G_PONG = 3;
constexpr uint8_t G_BREAKOUT = 4;
constexpr uint8_t G_SPACE = 5;
constexpr uint8_t G_FLAPPY = 6;
constexpr uint8_t G_DODGE = 7;
constexpr uint8_t G_2048 = 8;
constexpr uint8_t G_FROG = 9;
constexpr uint8_t G_MINES = 10;
constexpr uint8_t G_ASTRO = 11;

void gameUiBind(Adafruit_SSD1306 *display);
void gameUiDrawCentered(const char *txt, int16_t y);
void gameUiFramePlayfield();
void gameUiClearPlayfield();
void gameUiDrawPlayfieldBorder();
void gameUiDrawHudScore(uint8_t gameId, uint16_t score);
void gameUiDrawHudPair(uint8_t gameId, const char *left, uint16_t leftVal,
                       const char *right, uint16_t rightVal);
void gameUiDrawHudLabel(uint8_t gameId, const char *label, uint16_t val);
void gameUiDrawMenuHud();
void gameUiDrawMenuListFrame(int16_t y, int16_t h);
void gameUiDimPlayfield();
void gameUiDrawLose(GameEndInfo *info, uint16_t score);
void gameUiDrawWin(GameEndInfo *info, uint16_t score);
void gameUiDrawWinMsg(GameEndInfo *info, const char *title, uint16_t value,
                      ScoreKind kind);
