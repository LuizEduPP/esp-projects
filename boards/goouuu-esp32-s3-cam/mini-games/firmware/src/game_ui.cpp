#include "game_ui.h"

#include <cstdio>
#include <cstring>

static Adafruit_SSD1306 *gUi = nullptr;

void gameUiBind(Adafruit_SSD1306 *display) {
  gUi = display;
}

void gameUiDrawCentered(const char *txt, int16_t y) {
  if (!gUi || !txt) {
    return;
  }
  const int16_t x = (128 - static_cast<int16_t>(strlen(txt) * 6)) / 2;
  gUi->setCursor(x, y);
  gUi->print(txt);
}

static void drawHudStrip() {
  gUi->fillRect(0, 0, 128, GAME_UI_HUD_H, SSD1306_WHITE);
  gUi->fillRect(0, GAME_UI_HUD_H, 128, GAME_UI_SEP_H, SSD1306_BLACK);
}

static void printRec(uint8_t gameId) {
  const uint16_t rec = gameScoresGet(gameId);
  char buf[8];
  snprintf(buf, sizeof(buf), "R%u", rec);
  const int16_t x = 128 - static_cast<int16_t>(strlen(buf) * 6) - 2;
  gUi->setCursor(x, GAME_UI_TEXT_Y);
  gUi->print(buf);
}

static void drawHudBar(uint8_t gameId) {
  drawHudStrip();
  gUi->setTextColor(SSD1306_BLACK);
  gUi->setTextSize(1);
  printRec(gameId);
  gUi->setTextColor(SSD1306_WHITE);
}

void gameUiDrawHudScore(uint8_t gameId, uint16_t score) {
  if (!gUi) {
    return;
  }
  drawHudBar(gameId);
  gUi->setTextColor(SSD1306_BLACK);
  gUi->setCursor(2, GAME_UI_TEXT_Y);
  gUi->print(score);
  gUi->setTextColor(SSD1306_WHITE);
}

void gameUiDrawHudPair(uint8_t gameId, const char *left, uint16_t leftVal,
                       const char *right, uint16_t rightVal) {
  if (!gUi) {
    return;
  }
  char buf[12];
  drawHudBar(gameId);
  gUi->setTextColor(SSD1306_BLACK);
  gUi->setCursor(2, GAME_UI_TEXT_Y);
  snprintf(buf, sizeof(buf), "%s%u", left, leftVal);
  gUi->print(buf);
  if (right && right[0] != '\0') {
    gUi->setCursor(54, GAME_UI_TEXT_Y);
    snprintf(buf, sizeof(buf), "%s%u", right, rightVal);
    gUi->print(buf);
  }
  gUi->setTextColor(SSD1306_WHITE);
}

void gameUiDrawHudLabel(uint8_t gameId, const char *label, uint16_t val) {
  if (!gUi) {
    return;
  }
  char buf[12];
  drawHudBar(gameId);
  gUi->setTextColor(SSD1306_BLACK);
  gUi->setCursor(2, GAME_UI_TEXT_Y);
  snprintf(buf, sizeof(buf), "%s%u", label, val);
  gUi->print(buf);
  gUi->setTextColor(SSD1306_WHITE);
}

void gameUiDrawMenuHud() {
  if (!gUi) {
    return;
  }
  drawHudStrip();
  gUi->setTextColor(SSD1306_BLACK);
  gUi->setTextSize(1);
  gUi->setCursor(2, GAME_UI_TEXT_Y);
  gUi->print("GAMES");
  gUi->setCursor(80, GAME_UI_TEXT_Y);
  gUi->print("A=OK");
  gUi->setTextColor(SSD1306_WHITE);
}

void gameUiDrawMenuListFrame(int16_t y, int16_t h) {
  if (!gUi) {
    return;
  }
  gUi->fillRect(1, y + 1, 126, h - 2, SSD1306_BLACK);
  gUi->drawRect(0, y, 128, h, SSD1306_WHITE);
}

void gameUiClearPlayfield() {
  if (!gUi) {
    return;
  }
  gUi->fillRect(1, GAME_UI_TOP + 1, 126, GAME_UI_PLAY_H, SSD1306_BLACK);
}

void gameUiDrawPlayfieldBorder() {
  if (!gUi) {
    return;
  }
  gUi->drawRect(0, GAME_UI_TOP, 128, GAME_UI_H, SSD1306_WHITE);
}

void gameUiFramePlayfield() {
  gameUiClearPlayfield();
  gameUiDrawPlayfieldBorder();
}

void gameUiDimPlayfield() {
  gameUiClearPlayfield();
  gameUiDrawPlayfieldBorder();
}

static void drawEndCompact(bool won, uint16_t value, GameEndInfo *info, char metric,
                           const char *title) {
  constexpr int16_t kBoxH = 30;
  const int16_t boxY = GAME_UI_PLAY_Y + (GAME_UI_PLAY_H - kBoxH) / 2;
  gameUiDimPlayfield();
  gUi->drawRect(14, boxY, 100, kBoxH, SSD1306_WHITE);
  if (title && title[0] != '\0') {
    gameUiDrawCentered(title, boxY + 4);
  } else {
    gameUiDrawCentered(won ? "WIN!" : "OVER", boxY + 4);
  }
  char line[10];
  snprintf(line, sizeof(line), "%c:%u", metric, value);
  gameUiDrawCentered(line, boxY + 14);
  if (info && info->newRecord) {
    gameUiDrawCentered("REC!", boxY + 22);
  } else if (info && info->best > 0) {
    snprintf(line, sizeof(line), "R:%u", info->best);
    gameUiDrawCentered(line, boxY + 22);
  }
  gameUiDrawCentered("A=OK", GAME_UI_PLAY_BOTTOM - 7);
}

void gameUiDrawLose(GameEndInfo *info, uint16_t score) {
  if (!gUi) {
    return;
  }
  drawEndCompact(false, score, info, 'P', nullptr);
}

void gameUiDrawWin(GameEndInfo *info, uint16_t score) {
  if (!gUi) {
    return;
  }
  drawEndCompact(true, score, info, 'P', nullptr);
}

void gameUiDrawWinMsg(GameEndInfo *info, const char *title, uint16_t value,
                      ScoreKind kind) {
  if (!gUi) {
    return;
  }
  drawEndCompact(true, value, info, kind == ScoreKind::Lower ? 'M' : 'P', title);
}
