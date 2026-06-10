#include "game_scores.h"

#include <Preferences.h>
#include <stdio.h>

static Preferences gPrefs;
static bool gReady = false;

static void keyFor(uint8_t gameId, char *buf, size_t len) {
  snprintf(buf, len, "g%u", gameId);
}

void gameScoresBegin() {
  if (gReady) {
    return;
  }
  gPrefs.begin("minigames", false);
  gReady = true;
}

uint16_t gameScoresGet(uint8_t gameId) {
  if (!gReady) {
    gameScoresBegin();
  }
  char key[8];
  keyFor(gameId, key, sizeof(key));
  return gPrefs.getUShort(key, 0);
}

void gameEndReset(GameEndInfo *info) {
  if (!info) {
    return;
  }
  info->best = 0;
  info->newRecord = false;
  info->committed = false;
}

void gameEndCommit(GameEndInfo *info, uint8_t gameId, uint16_t value, ScoreKind kind,
                   bool ended) {
  if (!info) {
    return;
  }
  if (!ended) {
    info->committed = false;
    return;
  }
  if (info->committed) {
    return;
  }
  if (!gReady) {
    gameScoresBegin();
  }

  char key[8];
  keyFor(gameId, key, sizeof(key));
  const uint16_t cur = gPrefs.getUShort(key, 0);

  if (kind == ScoreKind::Higher) {
    info->best = cur;
    if (value > cur) {
      gPrefs.putUShort(key, value);
      info->best = value;
      info->newRecord = value > 0;
    }
  } else {
    info->best = cur;
    if (cur == 0 || value < cur) {
      gPrefs.putUShort(key, value);
      info->best = value;
      info->newRecord = value > 0;
    }
  }
  info->committed = true;
}
