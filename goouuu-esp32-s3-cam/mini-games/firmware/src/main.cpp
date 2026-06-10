#include <Arduino.h>
#include <esp_system.h>

#include "console_input.h"
#include "console_oled.h"
#include "game_system.h"

static bool gAppReady = false;

static const char *resetReasonStr() {
  switch (esp_reset_reason()) {
    case ESP_RST_POWERON:
      return "POWERON";
    case ESP_RST_BROWNOUT:
      return "BROWNOUT (3V3 drop?)";
    case ESP_RST_EXT:
      return "RESET pin";
    case ESP_RST_WDT:
      return "WDT";
    case ESP_RST_SW:
      return "SOFTWARE";
    case ESP_RST_PANIC:
      return "PANIC";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
      return "WATCHDOG";
    default:
      return "OTHER";
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.printf("Reset: %s (cod %d)\n", resetReasonStr(), static_cast<int>(esp_reset_reason()));
  Serial.println("=== ESP Mini Games ===");
  Serial.flush();

  if (!consoleOledBegin()) {
    Serial.println("OLED: init failed - VDD on 3V3? SDA=43 SCL=44");
    return;
  }

  consoleOledShow("Mini Games", "OLED OK", "A=OK", "");
  delay(300);

  consoleInputBegin();

  gameSystemBegin();
  consoleOledShow("5 buttons", "A=action", "hold L+R", "");
  delay(800);
  gAppReady = true;
}

void loop() {
  if (!gAppReady) {
    delay(500);
    return;
  }
  gameSystemLoop();
}
