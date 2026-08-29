#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <SPI.h>
#include <WiFi.h>

#include "pins.h"

#define COL_BG 0x0000
#define COL_CARD 0x18E3
#define COL_FG 0xFFFF
#define COL_DIM 0x7BEF
#define COL_FAINT 0x39E7
#define COL_ACCENT 0x07FF
#define COL_WARM 0xFD00
#define COL_COOL 0x347F
#define COL_GOOD 0x07E6
#define COL_BAD 0xF9A6
#define COL_SUN 0xFEA0
#define COL_CLOUD 0xC618
#define COL_RAIN 0x3D7F

static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
static GFXcanvas16 canvas(UI_W, UI_H);

static const char *const kWeekdays[] = {"DOMINGO", "SEGUNDA", "TERCA",  "QUARTA",
                                        "QUINTA",  "SEXTA",   "SABADO"};
static const char *const kMonths[] = {"jan", "fev", "mar", "abr", "mai", "jun",
                                      "jul", "ago", "set", "out", "nov", "dez"};

void uiBegin() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(2);
  tft.fillScreen(COL_BG);
  canvas.setTextWrap(false);
}

void uiPush() { tft.drawRGBBitmap(0, 0, canvas.getBuffer(), UI_W, UI_H); }

void uiScreenPower(bool on) {
  tft.enableDisplay(on);
  tft.enableSleep(!on);
}

static void textAt(const char *s, int x, int y, const GFXfont *font, uint16_t color) {
  canvas.setFont(font);
  canvas.setTextColor(color);
  canvas.setCursor(x, y);
  canvas.print(s);
}

static int textWidth(const char *s, const GFXfont *font) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.setFont(font);
  canvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

static void textCentered(const char *s, int y, const GFXfont *font, uint16_t color) {
  textAt(s, (UI_W - textWidth(s, font)) / 2, y, font, color);
}

static void smallAt(const char *s, int x, int y, uint16_t color) {
  canvas.setFont(nullptr);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  canvas.setCursor(x, y);
  canvas.print(s);
}

static void wifiGlyph(int x, int y, int bars) {
  for (int i = 0; i < 3; ++i) {
    const int h = 3 + i * 3;
    const uint16_t c = (i < bars) ? COL_GOOD : COL_FAINT;
    canvas.fillRect(x + i * 4, y + (9 - h), 3, h, c);
  }
}

static void statusBar(const char *title, int page) {
  canvas.fillScreen(COL_BG);
  canvas.fillRect(0, 0, UI_W, 15, COL_CARD);
  smallAt(title, 5, 4, COL_DIM);
  wifiGlyph(UI_W - 18, 3, netRssiBars());

  for (int i = 0; i < UI_PAGES; ++i) {
    const int x = 5 + i * 7;
    canvas.fillRect(x, 18, 5, 2, i == page ? COL_ACCENT : COL_FAINT);
  }
}

static void iconSun(int cx, int cy, int r) {
  canvas.fillCircle(cx, cy, r, COL_SUN);
  for (int i = 0; i < 8; ++i) {
    const float a = i * PI / 4;
    canvas.drawLine(cx + cos(a) * (r + 3), cy + sin(a) * (r + 3), cx + cos(a) * (r + 6),
                    cy + sin(a) * (r + 6), COL_SUN);
  }
}

static void iconCloud(int cx, int cy, uint16_t color) {
  canvas.fillCircle(cx - 7, cy + 2, 6, color);
  canvas.fillCircle(cx + 1, cy - 3, 8, color);
  canvas.fillCircle(cx + 9, cy + 2, 6, color);
  canvas.fillRect(cx - 7, cy + 2, 17, 6, color);
}

static void iconDrops(int cx, int cy, int count, uint16_t color) {
  for (int i = 0; i < count; ++i) {
    const int x = cx - 7 + i * 7;
    canvas.drawLine(x, cy, x - 2, cy + 6, color);
  }
}

static void weatherIcon(int code, int cx, int cy) {
  if (code <= 1) {
    iconSun(cx, cy, 11);
    return;
  }
  if (code == 2) {
    iconSun(cx + 6, cy - 6, 7);
    iconCloud(cx - 2, cy + 4, COL_CLOUD);
    return;
  }
  if (code == 3 || code == 45 || code == 48) {
    iconCloud(cx, cy, COL_CLOUD);
    return;
  }
  if (code >= 95) {
    iconCloud(cx, cy - 4, COL_DIM);
    canvas.fillTriangle(cx - 1, cy + 4, cx + 5, cy + 4, cx, cy + 14, COL_SUN);
    return;
  }
  if (code >= 71 && code <= 86) {
    iconCloud(cx, cy - 4, COL_CLOUD);
    iconDrops(cx, cy + 6, 3, COL_FG);
    return;
  }
  iconCloud(cx, cy - 4, COL_CLOUD);
  iconDrops(cx, cy + 6, 3, COL_RAIN);
}

static uint16_t tempColor(float t) {
  if (t >= 30) return COL_BAD;
  if (t >= 24) return COL_WARM;
  if (t <= 14) return COL_COOL;
  return COL_FG;
}

void uiSplash(const char *line1, const char *line2) {
  canvas.fillScreen(COL_BG);
  textCentered(line1, 62, &FreeSansBold24pt7b, COL_ACCENT);
  canvas.setFont(nullptr);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  const int w = strlen(line2) * 6;
  canvas.setCursor((UI_W - w) / 2, 82);
  canvas.print(line2);
  uiPush();
}

void uiProvisioning(bool armed) {
  canvas.fillScreen(COL_BG);
  textCentered("WI-FI", 30, &FreeSansBold12pt7b, COL_ACCENT);
  smallAt("Abra o app", 8, 46, COL_FG);
  smallAt("EspTouch / ESP-Touch", 8, 58, COL_FG);
  smallAt("no celular e envie", 8, 70, COL_FG);
  smallAt("a senha do Wi-Fi.", 8, 82, COL_FG);
  smallAt(armed ? "aguardando..." : "conectando...", 8, 102, COL_DIM);
  canvas.drawRect(4, 4, UI_W - 8, UI_H - 8, COL_FAINT);
  uiPush();
}

void uiClock(const struct tm &now, bool timeReady, int page) {
  statusBar("HORA", page);

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
  textCentered(hhmm, 62, &FreeSansBold24pt7b, timeReady ? COL_FG : COL_FAINT);

  const int barW = UI_W - 24;
  canvas.fillRect(12, 70, barW, 2, COL_FAINT);
  canvas.fillRect(12, 70, (barW * now.tm_sec) / 59, 2, COL_ACCENT);

  textCentered(kWeekdays[now.tm_wday % 7], 92, &FreeSans9pt7b, COL_ACCENT);

  char date[24];
  snprintf(date, sizeof(date), "%d de %s de %d", now.tm_mday, kMonths[now.tm_mon % 12],
           now.tm_year + 1900);
  canvas.setFont(nullptr);
  canvas.setTextSize(1);
  const int w = strlen(date) * 6;
  canvas.setTextColor(COL_DIM);
  canvas.setCursor((UI_W - w) / 2, 104);
  canvas.print(date);

  if (!timeReady) smallAt("sincronizando...", 26, 118, COL_WARM);
  uiPush();
}

void uiWeather(const Place &p, const Weather &w, int page) {
  statusBar("CLIMA", page);
  smallAt(p.city, 5, 26, COL_DIM);

  if (!w.valid) {
    weatherIcon(64, 66, 3);
    smallAt("sem dados", 34, 96, COL_WARM);
    uiPush();
    return;
  }

  weatherIcon(26, 56, w.code);

  char temp[8];
  snprintf(temp, sizeof(temp), "%.0f", w.tempC);
  const int tw = textWidth(temp, &FreeSansBold24pt7b);
  textAt(temp, 122 - tw - 10, 68, &FreeSansBold24pt7b, tempColor(w.tempC));
  textAt("C", 122 - 9, 48, &FreeSans9pt7b, COL_DIM);

  smallAt(w.desc, 5, 78, COL_FG);

  canvas.fillRect(0, 90, UI_W, 1, COL_FAINT);

  char line1[26];
  snprintf(line1, sizeof(line1), "min %.0f  max %.0f", w.minC, w.maxC);
  smallAt(line1, 5, 96, COL_DIM);

  char line2[26];
  snprintf(line2, sizeof(line2), "sens %.0f  umid %d%%", w.feelsC, w.humidity);
  smallAt(line2, 5, 108, COL_DIM);

  char line3[26];
  snprintf(line3, sizeof(line3), "chuva %d%%  vento %.0f", w.rainProb, w.windKph);
  smallAt(line3, 5, 120, w.rainProb >= 50 ? COL_RAIN : COL_DIM);
  uiPush();
}

void uiInsight(const char *text, bool pending, int page) {
  statusBar("AI", page);

  if (pending) {
    textCentered("...", 70, &FreeSansBold24pt7b, COL_FAINT);
    smallAt("pensando", 38, 96, COL_DIM);
    uiPush();
    return;
  }

  canvas.setFont(nullptr);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_FG);

  const int perLine = 20;
  int y = 32;
  const char *pp = text;
  while (*pp && y < UI_H - 8) {
    while (*pp == ' ') ++pp;
    if (!*pp) break;
    int take = 0;
    int lastSpace = -1;
    while (pp[take] && take < perLine) {
      if (pp[take] == ' ') lastSpace = take;
      ++take;
    }
    if (pp[take] && lastSpace > 0) take = lastSpace;
    canvas.setCursor(5, y);
    for (int i = 0; i < take; ++i) canvas.write(pp[i]);
    y += 11;
    pp += take;
  }
  uiPush();
}

void uiSystem(const Place &p, int page) {
  statusBar("SISTEMA", page);

  const int rows = 5;
  const char *labels[rows] = {"rede", "ip", "sinal", "livre", "ligado"};
  char values[rows][22];
  snprintf(values[0], sizeof(values[0]), "%s", netSsid());
  snprintf(values[1], sizeof(values[1]), "%s", netIp());
  snprintf(values[2], sizeof(values[2]), "%d dBm", (int)WiFi.RSSI());
  snprintf(values[3], sizeof(values[3]), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  const unsigned long up = millis() / 1000;
  snprintf(values[4], sizeof(values[4]), "%luh %02lum", up / 3600, (up % 3600) / 60);

  for (int i = 0; i < rows; ++i) {
    const int y = 30 + i * 15;
    smallAt(labels[i], 5, y, COL_FAINT);
    smallAt(values[i], 46, y, COL_FG);
  }

  smallAt("BOOT 3s = trocar rede", 5, 116, COL_DIM);
  uiPush();
}
