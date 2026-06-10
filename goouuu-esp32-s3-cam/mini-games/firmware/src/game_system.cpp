#include "game_system.h"

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <cmath>
#include <cstdio>
#include <cstring>

#include "console_input.h"
#include "console_oled.h"
#include "game_scores.h"
#include "game_ui.h"

struct GameEntry {
  const char *name;
  void (*run)();
};

static Adafruit_SSD1306 *gD = nullptr;
static uint32_t gLastFrame = 0;

static bool frameReady(uint16_t fps) {
  const uint32_t now = millis();
  if (now - gLastFrame < (1000 / fps)) {
    return false;
  }
  gLastFrame = now;
  return true;
}

static void drawCentered(const char *txt, int16_t y) {
  gameUiDrawCentered(txt, y);
}

static bool wantExit() {
  return consoleInputWantExit();
}

static bool btnA(const ConsoleInput &in) {
  return in.aPressed || in.aRepeat;
}

static bool actionA() {
  return consoleInputTakeA();
}

static bool dirUp(const ConsoleInput &in) {
  return in.upPressed || in.upRepeat;
}

static bool dirDown(const ConsoleInput &in) {
  return in.downPressed || in.downRepeat;
}

static bool dirLeft(const ConsoleInput &in) {
  return in.leftPressed || in.leftRepeat;
}

static bool dirRight(const ConsoleInput &in) {
  return in.rightPressed || in.rightRepeat;
}

// --- Snake ---
static void runSnake() {
  constexpr uint8_t GW = 32;
  constexpr uint8_t GH = 11;
  constexpr uint8_t CS = 4;
  uint8_t snakeX[128];
  uint8_t snakeY[128];
  uint16_t len = 3;
  int8_t dx = 1;
  int8_t dy = 0;
  uint8_t foodX = 10;
  uint8_t foodY = 5;
  uint16_t score = 0;
  bool over = false;
  GameEndInfo endInfo = {};

  for (uint16_t i = 0; i < len; i++) {
    snakeX[i] = 8 - i;
    snakeY[i] = 5;
  }

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      len = 3;
      dx = 1;
      dy = 0;
      over = false;
      score = 0;
      gameEndReset(&endInfo);
      for (uint16_t i = 0; i < len; i++) {
        snakeX[i] = 8 - i;
        snakeY[i] = 5;
      }
    }
    if (!over) {
      if (in.upPressed && dy == 0) {
        dx = 0;
        dy = -1;
      }
      if (in.downPressed && dy == 0) {
        dx = 0;
        dy = 1;
      }
      if (in.leftPressed && dx == 0) {
        dx = -1;
        dy = 0;
      }
      if (in.rightPressed && dx == 0) {
        dx = 1;
        dy = 0;
      }
    }

    if (!frameReady(10)) {
      continue;
    }

    if (!over) {
      for (uint16_t i = len; i > 0; i--) {
        snakeX[i] = snakeX[i - 1];
        snakeY[i] = snakeY[i - 1];
      }
      snakeX[0] = static_cast<uint8_t>(snakeX[0] + dx);
      snakeY[0] = static_cast<uint8_t>(snakeY[0] + dy);

      if (snakeX[0] >= GW || snakeY[0] >= GH) {
        over = true;
      }
      for (uint16_t i = 1; i < len; i++) {
        if (snakeX[0] == snakeX[i] && snakeY[0] == snakeY[i]) {
          over = true;
        }
      }
      if (!over && snakeX[0] == foodX && snakeY[0] == foodY) {
        if (len < 120) {
          len++;
        }
        score++;
        foodX = random(GW);
        foodY = random(GH);
      }
    }

    gameEndCommit(&endInfo, G_SNAKE, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    for (uint16_t i = 0; i < len; i++) {
      gD->fillRect(snakeX[i] * CS, snakeY[i] * CS + GAME_UI_PLAY_Y, CS - 1, CS - 1,
                   SSD1306_WHITE);
    }
    gD->fillRect(foodX * CS, foodY * CS + GAME_UI_PLAY_Y, CS - 1, CS - 1, SSD1306_WHITE);
    gameUiDrawHudScore(G_SNAKE, score);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Tetris ---
static const uint8_t TETRA[][4][4] = {
    {{0, 0, 0, 0}, {1, 1, 1, 1}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {{1, 1, 0, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {{0, 1, 1, 0}, {1, 1, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {{1, 1, 0, 0}, {0, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {{1, 0, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {{0, 0, 1, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
    {{0, 1, 0, 0}, {1, 1, 1, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}},
};

static bool tetraHit(const uint8_t grid[20][10], const uint8_t p[4][4], int8_t px, int8_t py) {
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (!p[y][x]) {
        continue;
      }
      const int8_t gx = px + x;
      const int8_t gy = py + y;
      if (gx < 0 || gx >= 10 || gy >= 20) {
        return true;
      }
      if (gy >= 0 && grid[gy][gx]) {
        return true;
      }
    }
  }
  return false;
}

static void runTetris() {
  uint8_t grid[20][10] = {};
  uint8_t piece[4][4] = {};
  int8_t px = 3;
  int8_t py = 0;
  uint8_t pType = 0;
  uint16_t score = 0;
  bool over = false;
  uint32_t dropAt = 0;
  uint16_t dropMs = 500;
  GameEndInfo endInfo = {};

  auto newPiece = [&]() {
    pType = random(7);
    memcpy(piece, TETRA[pType], sizeof(piece));
    px = 3;
    py = 0;
    if (tetraHit(grid, piece, px, py)) {
      over = true;
    }
  };
  newPiece();

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      memset(grid, 0, sizeof(grid));
      score = 0;
      over = false;
      dropMs = 500;
      gameEndReset(&endInfo);
      newPiece();
    }
    if (!over) {
      if (dirLeft(in) && !tetraHit(grid, piece, px - 1, py)) {
        px--;
      }
      if (dirRight(in) && !tetraHit(grid, piece, px + 1, py)) {
        px++;
      }
      if (in.upPressed) {
        uint8_t rot[4][4];
        for (uint8_t y = 0; y < 4; y++) {
          for (uint8_t x = 0; x < 4; x++) {
            rot[x][3 - y] = piece[y][x];
          }
        }
        if (!tetraHit(grid, rot, px, py)) {
          memcpy(piece, rot, sizeof(piece));
        }
      }
      if (actionA()) {
        while (!tetraHit(grid, piece, px, py + 1)) {
          py++;
        }
      }
    }

    if (!frameReady(30)) {
      continue;
    }

    if (!over) {
      if (in.down && !tetraHit(grid, piece, px, py + 1)) {
        py++;
      }
      if (millis() >= dropAt) {
        if (!tetraHit(grid, piece, px, py + 1)) {
          py++;
        } else {
          for (uint8_t y = 0; y < 4; y++) {
            for (uint8_t x = 0; x < 4; x++) {
              if (!piece[y][x] || py + y < 0) {
                continue;
              }
              grid[py + y][px + x] = 1;
            }
          }
          uint8_t lines = 0;
          for (int8_t y = 19; y >= 0; y--) {
            bool full = true;
            for (uint8_t x = 0; x < 10; x++) {
              if (!grid[y][x]) {
                full = false;
              }
            }
            if (full) {
              lines++;
              for (int8_t yy = y; yy > 0; yy--) {
                memcpy(grid[yy], grid[yy - 1], 10);
              }
              memset(grid[0], 0, 10);
              y++;
            }
          }
          if (lines) {
            score += lines * 100;
            dropMs = (500 - score / 5) < 120 ? 120 : static_cast<uint16_t>(500 - score / 5);
          }
          newPiece();
        }
        dropAt = millis() + dropMs;
      }
    }

    gameEndCommit(&endInfo, G_TETRIS, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    const int16_t ox = 24;
    const int16_t oy = GAME_UI_PLAY_Y;
    for (uint8_t y = 0; y < 20; y++) {
      for (uint8_t x = 0; x < 10; x++) {
        if (grid[y][x]) {
          gD->fillRect(ox + x * 4, oy + y * 2, 3, 1, SSD1306_WHITE);
        }
      }
    }
    for (uint8_t y = 0; y < 4; y++) {
      for (uint8_t x = 0; x < 4; x++) {
        if (piece[y][x] && py + y >= 0) {
          gD->fillRect(ox + (px + x) * 4, oy + (py + y) * 2, 3, 1, SSD1306_WHITE);
        }
      }
    }
    gameUiDrawHudScore(G_TETRIS, score);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Memory ---
static void runMemory() {
  uint8_t deck[8] = {0, 0, 1, 1, 2, 2, 3, 3};
  bool open[8] = {};
  bool solved[8] = {};
  uint8_t sel = 0;
  uint8_t first = 255;
  uint8_t moves = 0;
  bool lock = false;
  uint32_t lockUntil = 0;
  bool won = false;
  GameEndInfo endInfo = {};

  for (uint8_t i = 7; i > 0; i--) {
    const uint8_t j = random(i + 1);
    const uint8_t t = deck[i];
    deck[i] = deck[j];
    deck[j] = t;
  }

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (won && actionA()) {
      for (uint8_t i = 7; i > 0; i--) {
        const uint8_t j = random(i + 1);
        const uint8_t t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
      }
      memset(open, 0, sizeof(open));
      memset(solved, 0, sizeof(solved));
      first = 255;
      moves = 0;
      lock = false;
      won = false;
      gameEndReset(&endInfo);
    } else if (!won && in.up && in.down) {
      for (uint8_t i = 7; i > 0; i--) {
        const uint8_t j = random(i + 1);
        const uint8_t t = deck[i];
        deck[i] = deck[j];
        deck[j] = t;
      }
      memset(open, 0, sizeof(open));
      memset(solved, 0, sizeof(solved));
      first = 255;
      moves = 0;
      lock = false;
    }

    if (!lock) {
      if (dirLeft(in)) {
        sel = (sel + 7) % 8;
      }
      if (dirRight(in)) {
        sel = (sel + 1) % 8;
      }
      if (dirUp(in)) {
        sel = (sel + 4) % 8;
      }
      if (dirDown(in)) {
        sel = (sel + 4) % 8;
      }
      if (actionA() && !open[sel] && !solved[sel]) {
        open[sel] = true;
        if (first == 255) {
          first = sel;
        } else {
          moves++;
          if (deck[first] == deck[sel]) {
            solved[first] = solved[sel] = true;
            first = 255;
            bool all = true;
            for (uint8_t k = 0; k < 8; k++) {
              if (!solved[k]) {
                all = false;
              }
            }
            if (all) {
              won = true;
            }
          } else {
            lock = true;
            lockUntil = millis() + 600;
          }
        }
      }
    } else if (millis() >= lockUntil) {
      open[first] = open[sel] = false;
      first = 255;
      lock = false;
    }

    if (!frameReady(20)) {
      continue;
    }

    gameEndCommit(&endInfo, G_MEMORY, moves, ScoreKind::Lower, won);

    gameUiFramePlayfield();
    gameUiDrawHudPair(G_MEMORY, "M", moves, "", 0);
    for (uint8_t i = 0; i < 8; i++) {
      const int16_t cx = 2 + (i % 4) * 31;
      const int16_t cy = GAME_UI_PLAY_Y + 2 + (i / 4) * 20;
      if (i == sel) {
        gD->drawRect(cx - 1, cy - 1, 28, 16, SSD1306_WHITE);
      }
      gD->drawRect(cx, cy, 26, 14, SSD1306_WHITE);
      if (open[i] || solved[i]) {
        gD->setCursor(cx + 10, cy + 4);
        gD->print(deck[i] + 1);
      }
    }
    if (won) {
      gameUiDrawWinMsg(&endInfo, "OK!", moves, ScoreKind::Lower);
    }
    consoleOledFlush();
  }
}

// --- Pong ---
static void runPong() {
  constexpr int16_t kPaddleH = 20;
  constexpr int16_t kBallSz = 4;
  const float playMax = static_cast<float>(GAME_UI_PLAY_H - kPaddleH);
  const float ballMax = static_cast<float>(GAME_UI_PLAY_H - kBallSz);
  float py = playMax / 2.0f;
  float ey = playMax / 2.0f;
  float bx = 64;
  float by = static_cast<float>(GAME_UI_PLAY_H) / 2.0f;
  float bvx = 1.5f;
  float bvy = 1.0f;
  uint8_t pScore = 0;
  uint8_t eScore = 0;
  uint8_t bestRun = 0;
  float speed = 1.5f;
  GameEndInfo endInfo = {};

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      gameEndCommit(&endInfo, G_PONG, bestRun, ScoreKind::Higher, bestRun > 0);
      return;
    }

    const ConsoleInput &in = consoleInputState();

    if (!frameReady(30)) {
      continue;
    }

    if (in.up) {
      py -= 3;
    }
    if (in.down) {
      py += 3;
    }
    if (py < 0) {
      py = 0;
    }
    if (py > playMax) {
      py = playMax;
    }

    ey += (by < ey + 10) ? -2 : 2;
    if (ey < 0) {
      ey = 0;
    }
    if (ey > playMax) {
      ey = playMax;
    }

    bx += bvx;
    by += bvy;
    if (by <= 2 || by >= ballMax) {
      bvy = -bvy;
    }
    if (bx <= 8 && by >= py && by <= py + kPaddleH) {
      bvx = fabsf(bvx) + 0.1f;
      speed += 0.05f;
    }
    if (bx >= 120 && by >= ey && by <= ey + kPaddleH) {
      bvx = -fabsf(bvx) - 0.1f;
      speed += 0.05f;
    }
    if (bx < 0) {
      eScore++;
      bx = 64;
      by = static_cast<float>(GAME_UI_PLAY_H) / 2.0f;
      bvx = speed;
      bvy = (random(2) == 0) ? 1.0f : -1.0f;
    }
    if (bx > 128) {
      pScore++;
      if (pScore > bestRun) {
        bestRun = pScore;
      }
      bx = 64;
      by = static_cast<float>(GAME_UI_PLAY_H) / 2.0f;
      bvx = -speed;
      bvy = (random(2) == 0) ? 1.0f : -1.0f;
    }
    if (fabsf(bvx) > 4.0f) {
      bvx = (bvx > 0) ? 4.0f : -4.0f;
    }

    gameUiFramePlayfield();
    gD->fillRect(0, static_cast<int16_t>(py) + GAME_UI_PLAY_Y, 4, kPaddleH, SSD1306_WHITE);
    gD->fillRect(124, static_cast<int16_t>(ey) + GAME_UI_PLAY_Y, 4, kPaddleH, SSD1306_WHITE);
    gD->fillRect(static_cast<int16_t>(bx), static_cast<int16_t>(by) + GAME_UI_PLAY_Y, kBallSz,
                 kBallSz, SSD1306_WHITE);
    gameUiDrawHudPair(G_PONG, "ME", pScore, "PC", eScore);
    consoleOledFlush();
  }
}

// --- Breakout ---
static void runBreakout() {
  bool bricks[6][10] = {};
  for (uint8_t y = 0; y < 6; y++) {
    for (uint8_t x = 0; x < 10; x++) {
      bricks[y][x] = true;
    }
  }
  float px = 54;
  float bx = 64;
  constexpr int16_t kPaddleY = GAME_UI_PLAY_BOTTOM - 6;
  float by = kPaddleY - 8;
  float bvx = 1.2f;
  float bvy = -1.2f;
  uint8_t lives = 3;
  uint16_t bricksLeft = 60;
  uint16_t score = 0;
  bool over = false;
  bool won = false;
  GameEndInfo endInfo = {};

  auto resetBoard = [&]() {
    for (uint8_t y = 0; y < 6; y++) {
      for (uint8_t x = 0; x < 10; x++) {
        bricks[y][x] = true;
      }
    }
    px = 54;
    bx = 64;
    by = kPaddleY - 8;
    bvx = 1.2f;
    bvy = -1.2f;
    lives = 3;
    bricksLeft = 60;
    score = 0;
    over = false;
    won = false;
    gameEndReset(&endInfo);
  };

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if ((over || won) && actionA()) {
      resetBoard();
    }

    if (!frameReady(30)) {
      continue;
    }

    if (!over && !won) {
      if (in.left) {
        px -= 4;
      }
      if (in.right) {
        px += 4;
      }
      if (px < 0) {
        px = 0;
      }
      if (px > 108) {
        px = 108;
      }

      bx += bvx;
      by += bvy;
      if (bx <= 0 || bx >= 126) {
        bvx = -bvx;
      }
      if (by <= GAME_UI_PLAY_Y + 1) {
        bvy = -bvy;
      }
      if (by >= kPaddleY - 2 && bx >= px && bx <= px + 20) {
        bvy = -fabsf(bvy);
      }
      if (by > GAME_UI_BOTTOM) {
        if (lives > 0) {
          lives--;
        }
        bx = 64;
        by = kPaddleY - 8;
        bvx = 1.2f;
        bvy = -1.2f;
        if (lives == 0) {
          over = true;
        }
      }

      const int16_t gx = static_cast<int16_t>(bx / 12);
      const int16_t gy = static_cast<int16_t>((by - GAME_UI_PLAY_Y - 1) / 5);
      if (gy >= 0 && gy < 6 && gx >= 0 && gx < 10 && bricks[gy][gx]) {
        bricks[gy][gx] = false;
        bricksLeft--;
        score += 10;
        bvy = -bvy;
        if (bricksLeft == 0) {
          won = true;
        }
      }
    }

    gameEndCommit(&endInfo, G_BREAKOUT, score, ScoreKind::Higher, over || won);

    gameUiFramePlayfield();
    for (uint8_t y = 0; y < 6; y++) {
      for (uint8_t x = 0; x < 10; x++) {
        if (bricks[y][x]) {
          gD->fillRect(x * 12 + 2, GAME_UI_PLAY_Y + 1 + y * 5, 10, 3, SSD1306_WHITE);
        }
      }
    }
    gD->fillRect(static_cast<int16_t>(px), kPaddleY, 20, 4, SSD1306_WHITE);
    gD->fillRect(static_cast<int16_t>(bx), static_cast<int16_t>(by), 3, 3, SSD1306_WHITE);
    gameUiDrawHudPair(G_BREAKOUT, "P", score, "V", lives);
    if (won) {
      gameUiDrawWin(&endInfo, score);
    } else if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Space Impact ---
struct SpaceEnemy {
  int16_t x;
  int16_t y;
  bool alive;
};

static void runSpace() {
  int16_t px = 60;
  int16_t bullets[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
  SpaceEnemy enemies[8];
  uint16_t score = 0;
  uint8_t lives = 3;
  bool over = false;
  uint32_t spawnAt = 0;
  uint32_t fireCd = 0;
  GameEndInfo endInfo = {};

  for (uint8_t i = 0; i < 8; i++) {
    enemies[i].alive = false;
  }

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      score = 0;
      lives = 3;
      over = false;
      px = 60;
      gameEndReset(&endInfo);
      for (uint8_t i = 0; i < 8; i++) {
        enemies[i].alive = false;
        bullets[i] = -1;
      }
    }
    if (!over) {
      if (btnA(in) && millis() >= fireCd) {
        for (uint8_t i = 0; i < 8; i++) {
          if (bullets[i] < 0) {
            bullets[i] = GAME_UI_PLAY_BOTTOM - 6;
            fireCd = millis() + 160;
            break;
          }
        }
      }
    }

    if (!frameReady(25)) {
      continue;
    }

    if (!over) {
      if (in.left) {
        px -= 3;
      }
      if (in.right) {
        px += 3;
      }
      if (px < 0) {
        px = 0;
      }
      if (px > 120) {
        px = 120;
      }

      for (uint8_t i = 0; i < 8; i++) {
        if (bullets[i] >= 0) {
          bullets[i] -= 5;
          if (bullets[i] < 0) {
            bullets[i] = -1;
          }
        }
      }
      for (uint8_t i = 0; i < 8; i++) {
        if (!enemies[i].alive) {
          continue;
        }
        enemies[i].y += 2 + score / 300;
        if (enemies[i].y > GAME_UI_BOTTOM) {
          enemies[i].alive = false;
        }
        if (enemies[i].y > GAME_UI_PLAY_BOTTOM - 10 && abs(enemies[i].x - px) < 10) {
          enemies[i].alive = false;
          if (lives > 0) {
            lives--;
          }
          if (lives == 0) {
            over = true;
          }
        }
      }
      if (millis() >= spawnAt) {
        for (uint8_t i = 0; i < 8; i++) {
          if (!enemies[i].alive) {
            enemies[i].x = random(116);
            enemies[i].y = GAME_UI_TOP;
            enemies[i].alive = true;
            break;
          }
        }
        const uint32_t delayMs = (900 - score / 2) < 350 ? 350u : static_cast<uint32_t>(900 - score / 2);
        spawnAt = millis() + delayMs;
      }

      for (uint8_t bi = 0; bi < 8; bi++) {
        if (bullets[bi] < 0) {
          continue;
        }
        for (uint8_t ei = 0; ei < 8; ei++) {
          if (!enemies[ei].alive) {
            continue;
          }
          if (abs(px + 4 - enemies[ei].x) < 10 && abs(bullets[bi] - enemies[ei].y) < 8) {
            bullets[bi] = -1;
            enemies[ei].alive = false;
            score += 10;
          }
        }
      }
    }

    gameEndCommit(&endInfo, G_SPACE, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    if (!over) {
      gD->fillTriangle(px, GAME_UI_PLAY_BOTTOM - 2, px + 8, GAME_UI_PLAY_BOTTOM - 2,
                       px + 4, GAME_UI_PLAY_BOTTOM - 10, SSD1306_WHITE);
      for (uint8_t i = 0; i < 8; i++) {
        if (bullets[i] >= 0) {
          gD->fillRect(px + 3, bullets[i], 2, 4, SSD1306_WHITE);
        }
      }
      for (uint8_t i = 0; i < 8; i++) {
        if (enemies[i].alive) {
          gD->fillRect(enemies[i].x, enemies[i].y, 10, 6, SSD1306_WHITE);
        }
      }
    }
    gameUiDrawHudPair(G_SPACE, "P", score, "V", lives);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Flappy ---
static void runFlappy() {
  float birdY = GAME_UI_PLAY_Y + GAME_UI_PLAY_H / 2.0f;
  float birdVy = 0;
  int16_t pipeX = 128;
  int16_t gapY = GAME_UI_PLAY_Y + 10;
  uint16_t score = 0;
  bool over = false;
  uint8_t gapH = 20;
  GameEndInfo endInfo = {};

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      birdY = GAME_UI_PLAY_Y + GAME_UI_PLAY_H / 2.0f;
      birdVy = 0;
      pipeX = 128;
      gapY = GAME_UI_PLAY_Y + 8 + random(GAME_UI_PLAY_H - gapH - 16);
      score = 0;
      over = false;
      gameEndReset(&endInfo);
    }
    if (!over && actionA()) {
      birdVy = -2.8f;
    }

    if (!frameReady(20)) {
      continue;
    }

    if (!over) {
      birdVy += 0.22f;
      birdY += birdVy;
      pipeX -= 2;
      if (pipeX <= -12) {
        pipeX = 128;
        gapY = GAME_UI_PLAY_Y + 8 + random(GAME_UI_PLAY_H - gapH - 16);
        score++;
        if (gapH > 14) {
          gapH--;
        }
      }
      if (birdY < GAME_UI_PLAY_Y || birdY > GAME_UI_PLAY_BOTTOM - 4) {
        over = true;
      }
      if (pipeX > 20 && pipeX < 36) {
        if (birdY < gapY || birdY > gapY + gapH) {
          over = true;
        }
      }
    }

    gameEndCommit(&endInfo, G_FLAPPY, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    if (!over) {
      const int16_t topH = gapY > GAME_UI_PLAY_Y ? gapY - GAME_UI_PLAY_Y : 0;
      gD->fillRect(pipeX, GAME_UI_PLAY_Y, 10, topH, SSD1306_WHITE);
      gD->fillRect(pipeX, gapY + gapH, 10, GAME_UI_PLAY_BOTTOM - gapY - gapH + 1,
                   SSD1306_WHITE);
      gD->fillCircle(28, static_cast<int16_t>(birdY), 4, SSD1306_WHITE);
    }
    gameUiDrawHudScore(G_FLAPPY, score);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Dodge (corrida) ---
static void runDodge() {
  static constexpr int16_t LANES[3] = {8, 40, 72};
  uint8_t lane = 1;
  int16_t px = LANES[lane];
  int16_t ox[4] = {-1, -1, -1, -1};
  int16_t oy[4] = {0, 0, 0, 0};
  uint16_t score = 0;
  uint32_t spawnAt = 0;
  bool over = false;
  GameEndInfo endInfo = {};

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      lane = 1;
      px = LANES[lane];
      score = 0;
      over = false;
      gameEndReset(&endInfo);
      for (uint8_t i = 0; i < 4; i++) {
        ox[i] = -1;
      }
    }
    if (!over) {
      if (dirLeft(in) && lane > 0) {
        lane--;
      }
      if (dirRight(in) && lane < 2) {
        lane++;
      }
    }

    if (!frameReady(22)) {
      continue;
    }

    const int16_t target = LANES[lane];
    if (px < target) {
      px += 5;
      if (px > target) {
        px = target;
      }
    } else if (px > target) {
      px -= 5;
      if (px < target) {
        px = target;
      }
    }

    if (!over) {
      for (uint8_t i = 0; i < 4; i++) {
        if (ox[i] >= 0) {
          oy[i] += 3 + score / 80;
          if (oy[i] > GAME_UI_PLAY_BOTTOM + 8) {
            ox[i] = -1;
            score++;
          }
        }
      }
      if (millis() >= spawnAt) {
        for (uint8_t i = 0; i < 4; i++) {
          if (ox[i] < 0) {
            ox[i] = LANES[random(3)];
            oy[i] = GAME_UI_PLAY_Y;
            break;
          }
        }
        const uint32_t d = (700 - score * 3) < 280 ? 280u : static_cast<uint32_t>(700 - score * 3);
        spawnAt = millis() + d;
      }
      for (uint8_t i = 0; i < 4; i++) {
        if (ox[i] < 0) {
          continue;
        }
        if (oy[i] > GAME_UI_PLAY_BOTTOM - 14 && abs(px - ox[i]) < 14) {
          over = true;
        }
      }
    }

    gameEndCommit(&endInfo, G_DODGE, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    gD->drawFastHLine(0, GAME_UI_PLAY_BOTTOM - 10, 128, SSD1306_WHITE);
    if (!over) {
      gD->fillRect(px, GAME_UI_PLAY_BOTTOM - 8, 12, 8, SSD1306_WHITE);
      for (uint8_t i = 0; i < 4; i++) {
        if (ox[i] >= 0) {
          gD->fillRect(ox[i], oy[i], 14, 10, SSD1306_WHITE);
        }
      }
    }
    gameUiDrawHudScore(G_DODGE, score);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- 2048 ---
static void slide2048Line(uint16_t line[4], uint16_t &sc) {
  uint16_t out[4] = {0};
  uint8_t o = 0;
  uint8_t i = 0;
  while (i < 4) {
    if (!line[i]) {
      i++;
      continue;
    }
    if (i + 1 < 4 && line[i] == line[i + 1]) {
      out[o++] = static_cast<uint16_t>(line[i] * 2);
      sc += out[o - 1];
      i += 2;
    } else {
      out[o++] = line[i];
      i++;
    }
  }
  memcpy(line, out, sizeof(out));
}

static bool move2048(uint16_t grid[4][4], uint8_t dir, uint16_t &sc) {
  uint16_t before[4][4];
  memcpy(before, grid, sizeof(before));

  if (dir == 0) {
    for (uint8_t y = 0; y < 4; y++) {
      slide2048Line(grid[y], sc);
    }
  } else if (dir == 1) {
    for (uint8_t y = 0; y < 4; y++) {
      uint16_t line[4] = {grid[y][3], grid[y][2], grid[y][1], grid[y][0]};
      slide2048Line(line, sc);
      for (uint8_t x = 0; x < 4; x++) {
        grid[y][x] = line[3 - x];
      }
    }
  } else if (dir == 2) {
    for (uint8_t x = 0; x < 4; x++) {
      uint16_t line[4] = {grid[0][x], grid[1][x], grid[2][x], grid[3][x]};
      slide2048Line(line, sc);
      for (uint8_t y = 0; y < 4; y++) {
        grid[y][x] = line[y];
      }
    }
  } else {
    for (uint8_t x = 0; x < 4; x++) {
      uint16_t line[4] = {grid[3][x], grid[2][x], grid[1][x], grid[0][x]};
      slide2048Line(line, sc);
      for (uint8_t y = 0; y < 4; y++) {
        grid[y][x] = line[3 - y];
      }
    }
  }
  return memcmp(before, grid, sizeof(before)) != 0;
}

static void spawn2048(uint16_t grid[4][4]) {
  uint8_t slots[16];
  uint8_t n = 0;
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (!grid[y][x]) {
        slots[n++] = static_cast<uint8_t>(y * 4 + x);
      }
    }
  }
  if (!n) {
    return;
  }
  const uint8_t pick = slots[random(n)];
  grid[pick / 4][pick % 4] = (random(10) < 9) ? 2 : 4;
}

static bool canMove2048(const uint16_t grid[4][4]) {
  for (uint8_t y = 0; y < 4; y++) {
    for (uint8_t x = 0; x < 4; x++) {
      if (!grid[y][x]) {
        return true;
      }
      if (x + 1 < 4 && grid[y][x] == grid[y][x + 1]) {
        return true;
      }
      if (y + 1 < 4 && grid[y][x] == grid[y + 1][x]) {
        return true;
      }
    }
  }
  return false;
}

static void run2048() {
  uint16_t grid[4][4] = {};
  uint16_t score = 0;
  bool over = false;
  GameEndInfo endInfo = {};
  spawn2048(grid);
  spawn2048(grid);

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      memset(grid, 0, sizeof(grid));
      score = 0;
      over = false;
      gameEndReset(&endInfo);
      spawn2048(grid);
      spawn2048(grid);
    }

    if (!over) {
      uint8_t dir = 255;
      if (in.leftPressed) {
        dir = 0;
      } else if (in.rightPressed) {
        dir = 1;
      } else if (in.upPressed) {
        dir = 2;
      } else if (in.downPressed) {
        dir = 3;
      }
      if (dir < 4) {
        uint16_t add = 0;
        if (move2048(grid, dir, add)) {
          score += add;
          spawn2048(grid);
          if (!canMove2048(grid)) {
            over = true;
          }
        }
      }
    }

    if (!frameReady(20)) {
      continue;
    }

    gameEndCommit(&endInfo, G_2048, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    constexpr int16_t kCell = 10;
    constexpr int16_t kOx = (128 - kCell * 4) / 2;
    constexpr int16_t kOy = GAME_UI_PLAY_Y + (GAME_UI_PLAY_H - kCell * 4) / 2;
    for (uint8_t y = 0; y < 4; y++) {
      for (uint8_t x = 0; x < 4; x++) {
        const int16_t cx = kOx + x * kCell;
        const int16_t cy = kOy + y * kCell;
        gD->drawRect(cx, cy, kCell - 1, kCell - 1, SSD1306_WHITE);
        if (grid[y][x]) {
          char buf[6];
          snprintf(buf, sizeof(buf), "%u", grid[y][x]);
          gD->setCursor(cx + 1, cy + 2);
          gD->print(buf);
        }
      }
    }
    gameUiDrawHudScore(G_2048, score);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Frogger ---
static void runFrogger() {
  static constexpr int16_t kCols[5] = {12, 36, 60, 84, 108};
  static constexpr uint8_t kLanes = 5;
  int16_t cars[kLanes][2] = {{20, 90}, {100, 40}, {15, 75}, {95, 30}, {50, 110}};
  const int8_t cdir[kLanes] = {1, -1, 1, -1, 1};
  uint8_t col = 2;
  uint8_t row = kLanes;
  uint16_t score = 0;
  uint8_t lives = 3;
  bool over = false;
  GameEndInfo endInfo = {};

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      col = 2;
      row = kLanes;
      score = 0;
      lives = 3;
      over = false;
      gameEndReset(&endInfo);
    }

    if (!over) {
      if (in.leftPressed && col > 0) {
        col--;
      }
      if (in.rightPressed && col < 4) {
        col++;
      }
      if (in.upPressed && row > 0) {
        row--;
      }
      if (in.downPressed && row < kLanes) {
        row++;
      }
      if (row == 0) {
        score++;
        row = kLanes;
      }
    }

    if (!frameReady(18)) {
      continue;
    }

    if (!over) {
      for (uint8_t l = 0; l < kLanes; l++) {
        for (uint8_t c = 0; c < 2; c++) {
          cars[l][c] += cdir[l] * (2 + l / 2);
          if (cars[l][c] < -20) {
            cars[l][c] = 140;
          }
          if (cars[l][c] > 140) {
            cars[l][c] = -20;
          }
        }
      }
      if (row > 0 && row <= kLanes) {
        const int16_t fy = GAME_UI_PLAY_Y + 4 + row * 8;
        const int16_t fx = kCols[col];
        for (uint8_t l = 0; l < kLanes; l++) {
          if (row != l + 1) {
            continue;
          }
          for (uint8_t c = 0; c < 2; c++) {
            if (abs(fx - cars[l][c]) < 14) {
              row = kLanes;
              if (lives > 0) {
                lives--;
              }
              if (lives == 0) {
                over = true;
              }
            }
          }
        }
      }
    }

    gameEndCommit(&endInfo, G_FROG, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    gD->drawFastHLine(0, GAME_UI_PLAY_Y + 2, 128, SSD1306_WHITE);
    for (uint8_t l = 0; l < kLanes; l++) {
      const int16_t ly = GAME_UI_PLAY_Y + 6 + l * 8;
      for (uint8_t c = 0; c < 2; c++) {
        gD->fillRect(cars[l][c], ly, 18, 6, SSD1306_WHITE);
      }
    }
    const int16_t fx = kCols[col];
    const int16_t fy = GAME_UI_PLAY_Y + 4 + row * 8;
    gD->fillRect(fx - 4, fy, 8, 6, SSD1306_WHITE);
    gameUiDrawHudPair(G_FROG, "P", score, "V", lives);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

// --- Minesweeper ---
static uint8_t minesCountAdj(const bool mine[][9], uint8_t kW, uint8_t kH, uint8_t x,
                             uint8_t y) {
  uint8_t n = 0;
  for (int8_t dy = -1; dy <= 1; dy++) {
    for (int8_t dx = -1; dx <= 1; dx++) {
      if (!dx && !dy) {
        continue;
      }
      const int8_t nx = static_cast<int8_t>(x) + dx;
      const int8_t ny = static_cast<int8_t>(y) + dy;
      if (nx >= 0 && nx < static_cast<int8_t>(kW) && ny >= 0 &&
          ny < static_cast<int8_t>(kH) && mine[ny][nx]) {
        n++;
      }
    }
  }
  return n;
}

static void minesReveal(bool mine[][9], bool open[][9], uint8_t kW, uint8_t kH, uint8_t x,
                        uint8_t y, bool &over) {
  if (x >= kW || y >= kH || open[y][x]) {
    return;
  }
  open[y][x] = true;
  if (mine[y][x]) {
    over = true;
    return;
  }
  if (minesCountAdj(mine, kW, kH, x, y) == 0) {
    for (int8_t dy = -1; dy <= 1; dy++) {
      for (int8_t dx = -1; dx <= 1; dx++) {
        minesReveal(mine, open, kW, kH,
                    static_cast<uint8_t>(static_cast<int8_t>(x) + dx),
                    static_cast<uint8_t>(static_cast<int8_t>(y) + dy), over);
      }
    }
  }
}

static void runMinesweeper() {
  constexpr uint8_t kW = 9;
  constexpr uint8_t kH = 5;
  constexpr uint8_t kMines = 8;
  bool mine[kH][kW] = {};
  bool open[kH][kW] = {};
  uint8_t cx = 4;
  uint8_t cy = 2;
  bool seeded = false;
  bool over = false;
  bool won = false;
  GameEndInfo endInfo = {};

  auto placeMines = [&](uint8_t safeX, uint8_t safeY) {
    memset(mine, 0, sizeof(mine));
    uint8_t placed = 0;
    while (placed < kMines) {
      const uint8_t x = random(kW);
      const uint8_t y = random(kH);
      if ((x == safeX && y == safeY) || mine[y][x]) {
        continue;
      }
      mine[y][x] = true;
      placed++;
    }
  };

  auto checkWin = [&]() {
    for (uint8_t y = 0; y < kH; y++) {
      for (uint8_t x = 0; x < kW; x++) {
        if (!mine[y][x] && !open[y][x]) {
          return;
        }
      }
    }
    won = true;
  };

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if ((over || won) && actionA()) {
      memset(open, 0, sizeof(open));
      seeded = false;
      over = false;
      won = false;
      cx = 4;
      cy = 2;
      gameEndReset(&endInfo);
    }

    if (!over && !won) {
      if (dirLeft(in)) {
        if (cx > 0) {
          cx--;
        }
      }
      if (dirRight(in)) {
        if (cx + 1 < kW) {
          cx++;
        }
      }
      if (dirUp(in)) {
        if (cy > 0) {
          cy--;
        }
      }
      if (dirDown(in)) {
        if (cy + 1 < kH) {
          cy++;
        }
      }
      if (actionA()) {
        if (!seeded) {
          placeMines(cx, cy);
          seeded = true;
        }
        minesReveal(mine, open, kW, kH, cx, cy, over);
        checkWin();
      }
    }

    if (!frameReady(20)) {
      continue;
    }

    gameEndCommit(&endInfo, G_MINES, 1, ScoreKind::Higher, won);

    gameUiFramePlayfield();
    constexpr int16_t kCw = 13;
    constexpr int16_t kCh = 8;
    constexpr int16_t kOx = (128 - kCw * kW) / 2;
    constexpr int16_t kOy = GAME_UI_PLAY_Y + (GAME_UI_PLAY_H - kCh * kH) / 2;
    for (uint8_t y = 0; y < kH; y++) {
      for (uint8_t x = 0; x < kW; x++) {
        const int16_t px = kOx + x * kCw;
        const int16_t py = kOy + y * kCh;
        gD->drawRect(px, py, kCw - 1, kCh - 1, SSD1306_WHITE);
        if (open[y][x]) {
          if (mine[y][x]) {
            gD->fillRect(px + 2, py + 2, kCw - 5, kCh - 5, SSD1306_WHITE);
          } else {
            const uint8_t n = minesCountAdj(mine, kW, kH, x, y);
            if (n) {
              gD->setCursor(px + 4, py + 1);
              gD->print(n);
            }
          }
        }
      }
    }
    gD->drawRect(kOx + cx * kCw - 1, kOy + cy * kCh - 1, kCw + 1, kCh + 1, SSD1306_WHITE);
    gameUiDrawHudLabel(G_MINES, "OK", won ? 1u : 0u);
    if (won) {
      gameUiDrawWin(&endInfo, 1);
    } else if (over) {
      gameUiDrawLose(&endInfo, 0);
    }
    consoleOledFlush();
  }
}

// --- Asteroids ---
struct AstroRock {
  float x;
  float y;
  float vx;
  float vy;
  uint8_t size;
  bool alive;
};

static void runAsteroids() {
  float sx = 64;
  float sy = GAME_UI_PLAY_Y + GAME_UI_PLAY_H / 2.0f;
  float ang = -1.5708f;
  float svx = 0;
  float svy = 0;
  float bullets[4][3] = {};
  uint8_t bulletTtl[4] = {0, 0, 0, 0};
  AstroRock rocks[6];
  uint16_t score = 0;
  bool over = false;
  GameEndInfo endInfo = {};

  auto wrap = [&](float &x, float &y) {
    if (x < 2) {
      x = 126;
    }
    if (x > 126) {
      x = 2;
    }
    if (y < GAME_UI_PLAY_Y + 2) {
      y = GAME_UI_PLAY_BOTTOM - 2;
    }
    if (y > GAME_UI_PLAY_BOTTOM - 2) {
      y = GAME_UI_PLAY_Y + 2;
    }
  };

  auto spawnRocks = [&]() {
    for (uint8_t i = 0; i < 4; i++) {
      rocks[i].x = 20 + random(88);
      rocks[i].y = GAME_UI_PLAY_Y + 4 + random(GAME_UI_PLAY_H - 8);
      rocks[i].vx = (random(2) ? 1.0f : -1.0f) * (1.0f + random(10) / 10.0f);
      rocks[i].vy = (random(2) ? 1.0f : -1.0f) * (1.0f + random(10) / 10.0f);
      rocks[i].size = 2;
      rocks[i].alive = true;
    }
    for (uint8_t i = 4; i < 6; i++) {
      rocks[i].alive = false;
    }
  };
  spawnRocks();

  while (true) {
    consoleInputPoll();
    if (wantExit()) {
      return;
    }

    const ConsoleInput &in = consoleInputState();
    if (over && actionA()) {
      sx = 64;
      sy = GAME_UI_PLAY_Y + GAME_UI_PLAY_H / 2.0f;
      ang = -1.5708f;
      svx = svy = 0;
      score = 0;
      over = false;
      gameEndReset(&endInfo);
      memset(bulletTtl, 0, sizeof(bulletTtl));
      spawnRocks();
    }

    if (!frameReady(25)) {
      continue;
    }

    if (!over) {
      if (in.left) {
        ang -= 0.15f;
      }
      if (in.right) {
        ang += 0.15f;
      }
      if (in.up) {
        svx += cosf(ang) * 0.12f;
        svy += sinf(ang) * 0.12f;
      }
      if (actionA()) {
        for (uint8_t i = 0; i < 4; i++) {
          if (!bulletTtl[i]) {
            bullets[i][0] = sx;
            bullets[i][1] = sy;
            bullets[i][2] = ang;
            bulletTtl[i] = 40;
            break;
          }
        }
      }
      svx *= 0.98f;
      svy *= 0.98f;
      sx += svx;
      sy += svy;
      wrap(sx, sy);

      uint8_t alive = 0;
      for (uint8_t i = 0; i < 6; i++) {
        if (!rocks[i].alive) {
          continue;
        }
        alive++;
        rocks[i].x += rocks[i].vx;
        rocks[i].y += rocks[i].vy;
        wrap(rocks[i].x, rocks[i].y);
        const float dx = sx - rocks[i].x;
        const float dy = sy - rocks[i].y;
        const float r = rocks[i].size * 5.0f;
        if (dx * dx + dy * dy < r * r) {
          over = true;
        }
      }
      if (!alive) {
        spawnRocks();
      }

      for (uint8_t bi = 0; bi < 4; bi++) {
        if (!bulletTtl[bi]) {
          continue;
        }
        const float bang = bullets[bi][2];
        bullets[bi][0] += cosf(bang) * 3.0f;
        bullets[bi][1] += sinf(bang) * 3.0f;
        bulletTtl[bi]--;
        wrap(bullets[bi][0], bullets[bi][1]);
        for (uint8_t ri = 0; ri < 6; ri++) {
          if (!rocks[ri].alive) {
            continue;
          }
          const float dx = bullets[bi][0] - rocks[ri].x;
          const float dy = bullets[bi][1] - rocks[ri].y;
          const float r = rocks[ri].size * 5.0f;
          if (dx * dx + dy * dy < r * r) {
            bulletTtl[bi] = 0;
            const AstroRock hit = rocks[ri];
            rocks[ri].alive = false;
            score += 10;
            if (hit.size > 1) {
              uint8_t spawned = 0;
              for (uint8_t si = 0; si < 6 && spawned < 2; si++) {
                if (!rocks[si].alive) {
                  rocks[si] = hit;
                  rocks[si].size--;
                  rocks[si].vx = (spawned == 0 ? 1.5f : -1.5f);
                  rocks[si].vy = (spawned == 0 ? -1.2f : 1.2f);
                  rocks[si].alive = true;
                  spawned++;
                }
              }
            }
            break;
          }
        }
      }
    }

    gameEndCommit(&endInfo, G_ASTRO, score, ScoreKind::Higher, over);

    gameUiFramePlayfield();
    if (!over) {
      const int16_t nx = sx + static_cast<int16_t>(cosf(ang) * 6);
      const int16_t ny = sy + static_cast<int16_t>(sinf(ang) * 6);
      gD->drawLine(static_cast<int16_t>(sx), static_cast<int16_t>(sy), nx, ny, SSD1306_WHITE);
      for (uint8_t i = 0; i < 6; i++) {
        if (!rocks[i].alive) {
          continue;
        }
        const int16_t r = rocks[i].size * 4;
        gD->drawCircle(static_cast<int16_t>(rocks[i].x), static_cast<int16_t>(rocks[i].y), r,
                       SSD1306_WHITE);
      }
      for (uint8_t i = 0; i < 4; i++) {
        if (bulletTtl[i]) {
          gD->fillRect(static_cast<int16_t>(bullets[i][0]), static_cast<int16_t>(bullets[i][1]), 2,
                       2, SSD1306_WHITE);
        }
      }
    }
    gameUiDrawHudScore(G_ASTRO, score);
    if (over) {
      gameUiDrawLose(&endInfo, score);
    }
    consoleOledFlush();
  }
}

static const GameEntry GAMES[] = {
    {"Snake", runSnake},       {"Tetris", runTetris},     {"Memory", runMemory},
    {"Pong", runPong},         {"Breakout", runBreakout}, {"Space", runSpace},
    {"Flappy", runFlappy},     {"Dodge", runDodge},       {"2048", run2048},
    {"Frog", runFrogger},      {"Mines", runMinesweeper}, {"Astro", runAsteroids},
};
static constexpr uint8_t GAME_COUNT = sizeof(GAMES) / sizeof(GAMES[0]);

static constexpr uint8_t MENU_VISIBLE = 5;

static void drawMenu(uint8_t sel, uint8_t scroll) {
  if (!gD) {
    return;
  }
  gD->fillRect(0, 0, 128, 64, SSD1306_BLACK);
  gameUiDrawMenuHud();

  constexpr int16_t kListY = GAME_UI_TOP;
  constexpr int16_t kListH = GAME_UI_H;

  gameUiDrawMenuListFrame(kListY, kListH);

  for (uint8_t i = 0; i < MENU_VISIBLE; i++) {
    const uint8_t idx = scroll + i;
    if (idx >= GAME_COUNT) {
      break;
    }
    const int16_t y = kListY + 3 + i * 8;
    gD->setTextColor(SSD1306_WHITE);
    if (idx == sel) {
      gD->setCursor(4, y);
      gD->print(">");
      gD->setCursor(12, y);
    } else {
      gD->setCursor(12, y);
    }
    gD->print(GAMES[idx].name);
    const uint16_t rec = gameScoresGet(idx);
    if (rec > 0) {
      char buf[8];
      snprintf(buf, sizeof(buf), "%u", rec);
      const int16_t rx = 124 - static_cast<int16_t>(strlen(buf) * 6);
      gD->setCursor(rx, y);
      gD->print(buf);
    }
  }
  gD->setTextColor(SSD1306_WHITE);
  consoleOledFlush();
}

static void runMenu() {
  uint8_t sel = 0;
  uint8_t scroll = 0;
  uint8_t lastSel = 255;
  uint8_t lastScroll = 255;
  bool needRedraw = true;
  const uint32_t bootGuard = millis() + 600;

  Serial.println("Menu: ready");
  Serial.flush();

  drawMenu(sel, scroll);
  lastSel = sel;
  lastScroll = scroll;
  needRedraw = false;

  while (true) {
    consoleInputPoll();
    const ConsoleInput &in = consoleInputState();
    const bool inputReady = millis() >= bootGuard;

    if (inputReady && dirUp(in)) {
      if (sel > 0) {
        sel--;
      }
      if (sel < scroll) {
        scroll = sel;
      }
    }
    if (inputReady && dirDown(in)) {
      if (sel + 1 < GAME_COUNT) {
        sel++;
      }
      if (sel >= scroll + MENU_VISIBLE) {
        scroll = sel - MENU_VISIBLE + 1;
      }
    }
    if (inputReady && actionA()) {
      Serial.printf("Menu: game %u\n", sel);
      Serial.flush();
      GAMES[sel].run();
      needRedraw = true;
    }

    if (needRedraw || sel != lastSel || scroll != lastScroll) {
      drawMenu(sel, scroll);
      lastSel = sel;
      lastScroll = scroll;
      needRedraw = false;
    }

    delay(16);
  }
}

void gameSystemBegin() {
  gD = consoleOledDisplay();
  gameScoresBegin();
  gameUiBind(gD);
  randomSeed(esp_random());
}

void gameSystemLoop() {
  runMenu();
}
