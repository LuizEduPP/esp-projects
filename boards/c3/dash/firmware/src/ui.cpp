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
#define COL_FG 0xE73C
#define COL_DIM 0x7BCF
#define COL_FAINT 0x31A6
#define COL_HAIR 0x10A2
#define UI_PAD 6
#define UI_HEADER_Y 17
#define UI_FOOTER_Y 122
#define COL_ACCENT 0x279F
#define COL_WARM 0xFCAF
#define COL_COOL 0x62FF
#define COL_SUN 0xFE64
#define COL_CLOUD 0x94B2
#define COL_RAIN 0x3C1F

static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
static GFXcanvas16 canvas(UI_W, UI_H);

static const char *const kWeekdays[] = {"DOMINGO", "SEGUNDA", "TERCA",  "QUARTA",
                                        "QUINTA",  "SEXTA",   "SABADO"};
static const char *const kMonths[] = {"janeiro", "fevereiro", "marco",    "abril",
                                      "maio",    "junho",     "julho",    "agosto",
                                      "setembro", "outubro",  "novembro", "dezembro"};

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

static void micro(const char *s, int x, int y, uint16_t color) {
  canvas.setFont(nullptr);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  canvas.setCursor(x, y);
  canvas.print(s);
}

static int widthOf(const char *s, const GFXfont *font) {
  int16_t x1, y1;
  uint16_t w, h;
  canvas.setFont(font);
  canvas.getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  return w;
}

static void draw(const char *s, int x, int baseline, const GFXfont *font, uint16_t color) {
  canvas.setFont(font);
  canvas.setTextColor(color);
  canvas.setCursor(x, baseline);
  canvas.print(s);
}

static void drawCentered(const char *s, int baseline, const GFXfont *font, uint16_t color) {
  draw(s, (UI_W - widthOf(s, font)) / 2, baseline, font, color);
}

static void microRight(const char *s, int right, int y, uint16_t color) {
  micro(s, right - (int)strlen(s) * 6, y, color);
}

static void chrome(const char *label, int page) {
  canvas.fillScreen(COL_BG);
  micro(label, UI_PAD, 4, COL_DIM);

  const int bars = netRssiBars();
  for (int i = 0; i < 3; ++i) {
    const int h = 3 + i * 3;
    canvas.fillRect(UI_W - 20 + i * 5, 12 - h, 3, h, i < bars ? COL_ACCENT : COL_FAINT);
  }
  canvas.drawFastHLine(UI_PAD, UI_HEADER_Y, UI_W - 2 * UI_PAD, COL_FAINT);

  const int seg = (UI_W - 2 * UI_PAD) / UI_PAGES;
  for (int i = 0; i < UI_PAGES; ++i) {
    canvas.fillRect(UI_PAD + i * seg, UI_FOOTER_Y, seg - 4, 2,
                    i == page ? COL_ACCENT : COL_FAINT);
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
  canvas.fillCircle(cx - 7, cy + 3, 6, color);
  canvas.fillCircle(cx + 1, cy - 3, 8, color);
  canvas.fillCircle(cx + 9, cy + 3, 6, color);
  canvas.fillRect(cx - 7, cy + 2, 17, 7, color);
}

static void iconRain(int cx, int cy, uint16_t color) {
  iconCloud(cx, cy - 4, COL_CLOUD);
  for (int i = 0; i < 3; ++i) {
    const int x = cx - 8 + i * 8;
    canvas.drawLine(x, cy + 8, x - 3, cy + 15, color);
  }
}

static void weatherIcon(int code, int cx, int cy) {
  if (code <= 1) {
    iconSun(cx, cy, 10);
  } else if (code == 2) {
    iconSun(cx + 7, cy - 7, 6);
    iconCloud(cx - 2, cy + 3, COL_CLOUD);
  } else if (code == 3 || code == 45 || code == 48) {
    iconCloud(cx, cy, COL_CLOUD);
  } else if (code >= 95) {
    iconCloud(cx, cy - 5, COL_DIM);
    canvas.fillTriangle(cx - 2, cy + 5, cx + 4, cy + 5, cx - 1, cy + 16, COL_SUN);
  } else if (code >= 71 && code <= 86) {
    iconRain(cx, cy, COL_FG);
  } else {
    iconRain(cx, cy, COL_RAIN);
  }
}

static uint16_t tempColor(float t) {
  if (t >= 24) return COL_WARM;
  if (t <= 15) return COL_COOL;
  return COL_FG;
}

void uiSplash(const char *line1, const char *line2) {
  canvas.fillScreen(COL_BG);
  drawCentered(line1, 68, &FreeSansBold24pt7b, COL_ACCENT);
  micro(line2, (UI_W - (int)strlen(line2) * 6) / 2, 86, COL_DIM);
  uiPush();
}

void uiProvisioning(bool armed) {
  canvas.fillScreen(COL_BG);
  micro("WI-FI", 6, 5, COL_DIM);
  draw("Conecte", 8, 40, &FreeSansBold12pt7b, COL_ACCENT);
  micro("Abra o app EspTouch", 8, 54, COL_FG);
  micro("no celular e envie a", 8, 66, COL_FG);
  micro("senha do Wi-Fi.", 8, 78, COL_FG);
  micro(armed ? "aguardando" : "conectando", 8, 100, COL_DIM);
  canvas.fillRect(6, 112, UI_W - 12, 2, COL_FAINT);
  uiPush();
}

void uiClock(const struct tm &now, bool timeReady, int page) {
  chrome(DASH_CITY, page);

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
  drawCentered(hhmm, 62, &FreeSansBold24pt7b, timeReady ? COL_FG : COL_FAINT);

  drawCentered(kWeekdays[now.tm_wday % 7], 90, &FreeSansBold12pt7b, COL_ACCENT);

  if (timeReady) {
    char date[24];
    snprintf(date, sizeof(date), "%d de %s", now.tm_mday, kMonths[now.tm_mon % 12]);
    drawCentered(date, 110, &FreeSans9pt7b, COL_DIM);
  } else {
    drawCentered("sincronizando", 110, &FreeSans9pt7b, COL_WARM);
  }
  uiPush();
}

void uiWeather(const Place &p, const Weather &w, int page) {
  chrome("CLIMA", page);

  if (!w.valid) {
    iconCloud(64, 56, COL_FAINT);
    drawCentered("sem dados", 92, &FreeSans9pt7b, COL_DIM);
    uiPush();
    return;
  }

  char temp[6];
  snprintf(temp, sizeof(temp), "%.0f", w.tempC);
  const uint16_t tc = tempColor(w.tempC);
  draw(temp, UI_PAD, 62, &FreeSansBold24pt7b, tc);
  canvas.drawCircle(UI_PAD + widthOf(temp, &FreeSansBold24pt7b) + 6, 36, 3, tc);

  weatherIcon(w.code, 100, 46);

  draw(w.desc, UI_PAD, 82, &FreeSans9pt7b, COL_FG);
  canvas.drawFastHLine(UI_PAD, 92, UI_W - 2 * UI_PAD, COL_FAINT);

  const char *labels[3] = {"MIN", "MAX", "UMID"};
  char values[3][8];
  snprintf(values[0], sizeof(values[0]), "%.0f", w.minC);
  snprintf(values[1], sizeof(values[1]), "%.0f", w.maxC);
  snprintf(values[2], sizeof(values[2]), "%d%%", w.humidity);

  const int step = (UI_W - 2 * UI_PAD) / 3;
  for (int i = 0; i < 3; ++i) {
    const int cx = UI_PAD + step * i + step / 2;
    micro(labels[i], cx - (int)strlen(labels[i]) * 3, 98, COL_FAINT);
    draw(values[i], cx - widthOf(values[i], &FreeSans9pt7b) / 2, 118, &FreeSans9pt7b, COL_FG);
  }
  uiPush();
}

void uiInsight(const char *text, bool pending, int page) {
  chrome("AI", page);

  if (pending) {
    drawCentered("...", 70, &FreeSansBold24pt7b, COL_FAINT);
    uiPush();
    return;
  }

  const int kMaxLines = 6;
  char lines[kMaxLines][26];
  int count = 0;

  const char *p = text;
  while (*p && count < kMaxLines) {
    while (*p == ' ') ++p;
    if (!*p) break;

    char probe[26];
    int take = 0;
    int fit = 0;
    while (p[take]) {
      const int len = take + 1;
      if (len >= (int)sizeof(probe)) break;
      memcpy(probe, p, len);
      probe[len] = '\0';
      if (widthOf(probe, &FreeSans9pt7b) > UI_W - 24) break;
      if (p[take] == ' ') fit = take;
      ++take;
    }
    if (p[take] && fit > 0) take = fit;

    memcpy(lines[count], p, take);
    lines[count][take] = '\0';
    ++count;
    p += take;
  }

  const int block = count * 15;
  const int top = 24 + (94 - block > 0 ? (94 - block) / 2 : 0);
  canvas.fillRect(UI_PAD, top, 2, block - 4, COL_ACCENT);
  for (int i = 0; i < count; ++i) {
    draw(lines[i], UI_PAD + 8, top + 11 + i * 15, &FreeSans9pt7b, COL_FG);
  }
  uiPush();
}

void uiSystem(const Place &p, int page) {
  chrome("SISTEMA", page);

  const char *labels[5] = {"rede", "ip", "sinal", "livre", "ligado"};
  char values[5][22];
  snprintf(values[0], sizeof(values[0]), "%s", netSsid());
  snprintf(values[1], sizeof(values[1]), "%s", netIp());
  snprintf(values[2], sizeof(values[2]), "%d dBm", (int)WiFi.RSSI());
  snprintf(values[3], sizeof(values[3]), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  const unsigned long up = millis() / 1000;
  snprintf(values[4], sizeof(values[4]), "%luh %02lum", up / 3600, (up % 3600) / 60);

  for (int i = 0; i < 5; ++i) {
    const int y = 27 + i * 18;
    micro(labels[i], UI_PAD, y, COL_FAINT);
    microRight(values[i], UI_W - UI_PAD, y, COL_FG);
    if (i < 4) canvas.drawFastHLine(UI_PAD, y + 13, UI_W - 2 * UI_PAD, COL_HAIR);
  }
  uiPush();
}
