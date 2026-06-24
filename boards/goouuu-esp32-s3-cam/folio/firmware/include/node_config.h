#pragma once

#include <Arduino.h>

struct NodeRuntimeConfig {
  char version[16];
  uint32_t frameIntervalMs;
  uint8_t jpegQuality;
  uint8_t frameSizeId;
  uint16_t audioChunkMs;
  uint16_t audioSampleRate;
  uint32_t wifiRetryMs;
  uint32_t pushBackoffMaxMs;
  uint32_t statusIntervalMs;
  uint32_t motionCaptureMinMs;
  float motionMin;
  float soundMinEnergy;
  float speechEnergyThreshold;
};

void nodeConfigBegin();
bool nodeConfigPoll();
const NodeRuntimeConfig &nodeConfig();
const char *nodeConfigVersionHeader();
/** Push URL — compile-time default until brain syncs brainUrl on same subnet. */
const char *nodeBrainUrl();
