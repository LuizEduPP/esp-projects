#include "console_oled.h"

#include <Wire.h>

#include "pins.h"

static constexpr uint16_t OLED_W = 128;
static constexpr uint16_t OLED_H = 64;

static Adafruit_SSD1306 gDisplay(OLED_W, OLED_H, &Wire, -1);
static bool gReady = false;

static bool probeI2c(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

static uint8_t findOledAddr() {
  if (probeI2c(0x3C)) {
    return 0x3C;
  }
  if (probeI2c(0x3D)) {
    return 0x3D;
  }
  return 0;
}

static void logI2cProbe() {
  Serial.printf("I2C probe SDA=%d SCL=%d:", PIN_OLED_SDA, PIN_OLED_SCL);
  const uint8_t addr = findOledAddr();
  if (addr != 0) {
    Serial.printf(" 0x%02X", addr);
  } else {
    Serial.print(" (none)");
  }
  Serial.println();
  Serial.flush();
}

bool consoleOledBegin() {
  gReady = false;
  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(50000);
#ifdef WIRE_HAS_TIMEOUT
  Wire.setWireTimeout(25000, true);
#endif
  delay(50);

  logI2cProbe();

  const uint8_t addr = findOledAddr();
  if (addr == 0) {
    Serial.println("OLED: empty I2C bus - check VDD 3V3 and SDA/SCL wires");
    return false;
  }

  if (!gDisplay.begin(SSD1306_SWITCHCAPVCC, addr)) {
    gDisplay.clearDisplay();
    if (!gDisplay.begin(SSD1306_EXTERNALVCC, addr)) {
      Serial.printf("OLED: device at 0x%02X but init failed\n", addr);
      return false;
    }
    Serial.printf("OLED: OK 0x%02X EXTERNALVCC\n", addr);
  } else {
    Serial.printf("OLED: OK 0x%02X SWITCHCAPVCC\n", addr);
  }

  gDisplay.ssd1306_command(SSD1306_DISPLAYON);
  gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
  gDisplay.ssd1306_command(0xFF);
  gDisplay.clearDisplay();
  gDisplay.setTextColor(SSD1306_WHITE);
  gDisplay.setTextSize(1);
  gDisplay.display();
  gReady = true;
  Serial.flush();
  return true;
}

Adafruit_SSD1306 *consoleOledDisplay() {
  return gReady ? &gDisplay : nullptr;
}

void consoleOledClear() {
  if (!gReady) {
    return;
  }
  gDisplay.clearDisplay();
}

void consoleOledFlush() {
  if (!gReady) {
    return;
  }
  gDisplay.display();
  static uint32_t lastKeepalive = 0;
  const uint32_t now = millis();
  if (now - lastKeepalive >= 2000) {
    lastKeepalive = now;
    gDisplay.ssd1306_command(SSD1306_DISPLAYON);
    gDisplay.ssd1306_command(SSD1306_SETCONTRAST);
    gDisplay.ssd1306_command(0xFF);
  }
}

void consoleOledShow(const char *line1, const char *line2, const char *line3, const char *line4) {
  if (!gReady) {
    return;
  }
  gDisplay.clearDisplay();
  gDisplay.setCursor(0, 0);
  if (line1) {
    gDisplay.println(line1);
  }
  if (line2) {
    gDisplay.println(line2);
  }
  if (line3) {
    gDisplay.println(line3);
  }
  if (line4) {
    gDisplay.println(line4);
  }
  gDisplay.display();
}
