#pragma once

#include <stdint.h>

enum class ScoreKind : uint8_t { Higher, Lower };

struct GameEndInfo {
  uint16_t best = 0;
  bool newRecord = false;
  bool committed = false;
};

void gameScoresBegin();
uint16_t gameScoresGet(uint8_t gameId);
void gameEndReset(GameEndInfo *info);
void gameEndCommit(GameEndInfo *info, uint8_t gameId, uint16_t value, ScoreKind kind,
                  bool ended);
