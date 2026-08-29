#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>
#include <SPI.h>
#include <WiFi.h>

#include "pins.h"

#define COL_BG 0x0000
#define COL_CARD 0x10C4
#define COL_EDGE 0x2166
#define COL_FG 0xFFFF
#define COL_DIM 0x9D36
#define COL_FAINT 0x3209
#define COL_NIGHT 0x2104
#define COL_ACCENT 0x073F
#define COL_HOT 0xFC40
#define COL_COLD 0x2DBE
#define COL_GREEN 0x072E
#define COL_PINK 0xF8B2
#define COL_SUN 0xFEA0
#define COL_CLOUD 0xAD75
#define COL_RAIN 0x2DBE

#define UI_PAD 6
#define UI_FOOTER_Y 122

static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
static GFXcanvas16 canvas(UI_W, UI_H);
static uint8_t sIntro = 0;

static const char *const kWeekdays[] = {"DOMINGO", "SEGUNDA", "TERCA",  "QUARTA",
                                        "QUINTA",  "SEXTA",   "SABADO"};
static const char *const kMonths[] = {"jan", "fev", "mar", "abr", "mai", "jun",
                                      "jul", "ago", "set", "out", "nov", "dez"};

void uiBegin() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_144GREENTAB);
  tft.setSPISpeed(20000000);
  tft.setRotation(2);
  tft.fillScreen(COL_BG);
  canvas.setTextWrap(false);
}

static void scaleBuffer(uint8_t num, uint8_t den) {
  uint16_t *buf = canvas.getBuffer();
  for (int i = 0; i < UI_W * UI_H; ++i) {
    const uint16_t c = buf[i];
    if (!c) continue;
    uint16_t r = ((c >> 11) & 0x1F) * num / den;
    uint16_t g = ((c >> 5) & 0x3F) * num / den;
    uint16_t b = (c & 0x1F) * num / den;
    if (r > 0x1F) r = 0x1F;
    if (g > 0x3F) g = 0x3F;
    if (b > 0x1F) b = 0x1F;
    buf[i] = (r << 11) | (g << 5) | b;
  }
}

void uiPush() {
  if (sIntro) {
    sIntro = 0;
    scaleBuffer(1, 5);
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), UI_W, UI_H);
    scaleBuffer(5, 2);
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), UI_W, UI_H);
    scaleBuffer(5, 2);
  }
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), UI_W, UI_H);
}

void uiIntro() { sIntro = 1; }

static void micro(const char *s, int x, int y, uint16_t color) {
  canvas.setFont(nullptr);
  canvas.setTextSize(1);
  canvas.setTextColor(color);
  canvas.setCursor(x, y);
  canvas.print(s);
}

static void microRight(const char *s, int right, int y, uint16_t color) {
  micro(s, right - (int)strlen(s) * 6, y, color);
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

static void drawRight(const char *s, int right, int baseline, const GFXfont *font,
                      uint16_t color) {
  draw(s, right - widthOf(s, font), baseline, font, color);
}

static void drawCentered(const char *s, int baseline, const GFXfont *font, uint16_t color) {
  draw(s, (UI_W - widthOf(s, font)) / 2, baseline, font, color);
}

static int card(int x, int y, int w, int h, const char *label) {
  canvas.fillRoundRect(x, y, w, h, 4, COL_CARD);
  canvas.drawRoundRect(x, y, w, h, 4, COL_EDGE);
  if (!label) return y + 8;
  micro(label, x + UI_PAD, y + 6, COL_DIM);
  return y + 17;
}

static void pager(int page) {
  const int seg = (UI_W - 2 * UI_PAD) / UI_PAGES;
  for (int i = 0; i < UI_PAGES; ++i) {
    canvas.fillRect(UI_PAD + i * seg, UI_FOOTER_Y, seg - 4, 2,
                    i == page ? COL_ACCENT : COL_FAINT);
  }
}

static void signalBars(int x, int y, int bars, unsigned long t) {
  const int pulse = (t / 300) % 4;
  for (int i = 0; i < 3; ++i) {
    const int h = 3 + i * 3;
    uint16_t c = COL_FAINT;
    if (i < bars) c = COL_ACCENT;
    else if (bars == 0 && i == pulse) c = COL_DIM;
    canvas.fillRect(x + i * 5, y - h, 3, h, c);
  }
}

static void iconSun(int cx, int cy, unsigned long t) {
  const float a0 = (t % 8000) * (PI / 4000.0f);
  const int r = 6 + ((t / 400) % 2);
  for (int i = 0; i < 8; ++i) {
    const float a = a0 + i * PI / 4;
    canvas.drawLine(cx + cos(a) * (r + 2), cy + sin(a) * (r + 2), cx + cos(a) * (r + 6),
                    cy + sin(a) * (r + 6), COL_SUN);
  }
  canvas.fillCircle(cx, cy, r, COL_SUN);
}

static void puff(int cx, int cy, uint16_t color) {
  canvas.fillCircle(cx - 6, cy + 2, 5, color);
  canvas.fillCircle(cx + 1, cy - 3, 7, color);
  canvas.fillCircle(cx + 8, cy + 2, 5, color);
  canvas.fillRect(cx - 6, cy + 1, 15, 6, color);
}

static void iconCloud(int cx, int cy, unsigned long t, uint16_t color) {
  const int drift = (int)(sin(t / 900.0f) * 2.0f);
  puff(cx + drift, cy, color);
}

static void iconRain(int cx, int cy, unsigned long t, uint16_t color) {
  iconCloud(cx, cy - 4, t, COL_CLOUD);
  for (int i = 0; i < 3; ++i) {
    const int phase = (t / 55 + i * 7) % 18;
    const int y = cy + 5 + phase / 2;
    if (phase < 14) canvas.drawLine(cx - 7 + i * 7, y, cx - 9 + i * 7, y + 4, color);
  }
}

static void iconStorm(int cx, int cy, unsigned long t) {
  iconCloud(cx, cy - 4, t, COL_DIM);
  if ((t / 220) % 5) canvas.fillTriangle(cx - 1, cy + 4, cx + 5, cy + 4, cx, cy + 15, COL_SUN);
}

static void iconSnow(int cx, int cy, unsigned long t) {
  iconCloud(cx, cy - 4, t, COL_CLOUD);
  for (int i = 0; i < 3; ++i) {
    const int phase = (t / 90 + i * 6) % 16;
    canvas.fillCircle(cx - 7 + i * 7, cy + 5 + phase / 2, 1, COL_FG);
  }
}

static void weatherIcon(int code, int cx, int cy, unsigned long t) {
  if (code <= 1) {
    iconSun(cx, cy, t);
  } else if (code == 2) {
    iconSun(cx + 6, cy - 6, t);
    iconCloud(cx - 2, cy + 3, t, COL_CLOUD);
  } else if (code == 3 || code == 45 || code == 48) {
    iconCloud(cx, cy, t, COL_CLOUD);
  } else if (code >= 95) {
    iconStorm(cx, cy, t);
  } else if (code >= 71 && code <= 86) {
    iconSnow(cx, cy, t);
  } else {
    iconRain(cx, cy, t, COL_RAIN);
  }
}

static uint16_t tempColor(float t) {
  if (t >= 28) return COL_HOT;
  if (t >= 20) return COL_SUN;
  if (t <= 12) return COL_COLD;
  return COL_GREEN;
}

void uiSplash(const char *line1, const char *line2) {
  canvas.fillScreen(COL_BG);
  drawCentered(line1, 66, &FreeSansBold24pt7b, COL_ACCENT);
  micro(line2, (UI_W - (int)strlen(line2) * 6) / 2, 82, COL_DIM);
  uiPush();
}

void uiProvisioning(bool armed) {
  const unsigned long t = millis();
  canvas.fillScreen(COL_BG);

  int y = card(4, 4, 120, 58, "WI-FI");
  draw("Conecte", 4 + UI_PAD, y + 14, &FreeSansBold12pt7b, COL_ACCENT);
  micro("app EspTouch no celular", 4 + UI_PAD, y + 24, COL_DIM);

  y = card(4, 68, 120, 50, nullptr);
  micro(armed ? "aguardando senha" : "conectando", 4 + UI_PAD, y + 4, COL_FG);
  const int w = 108;
  const int fill = (t / 20) % w;
  canvas.fillRect(4 + UI_PAD, y + 22, w, 3, COL_FAINT);
  canvas.fillRect(4 + UI_PAD, y + 22, fill, 3, COL_ACCENT);
  uiPush();
}

void uiClock(const struct tm &now, bool timeReady, int page) {
  const unsigned long t = millis();
  canvas.fillScreen(COL_BG);

  card(4, 4, 120, 58, nullptr);

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
  drawCentered(hhmm, 45, &FreeSansBold24pt7b, timeReady ? COL_FG : COL_FAINT);

  if (timeReady) {
    const int w = 108 * now.tm_sec / 59;
    canvas.fillRect(10, 52, 108, 2, COL_FAINT);
    canvas.fillRect(10, 52, w, 2, COL_ACCENT);
  }
  if (!netOnline()) canvas.fillCircle(114, 12, 3, COL_PINK);

  int y = card(4, 66, 66, 52, "DIA");
  draw(timeReady ? kWeekdays[now.tm_wday % 7] : "--", 4 + UI_PAD, y + 20, &FreeSansBold9pt7b,
       COL_ACCENT);

  y = card(74, 66, 50, 52, "DATA");
  char date[8];
  snprintf(date, sizeof(date), "%02d/%s", now.tm_mday, kMonths[now.tm_mon % 12]);
  draw(timeReady ? date : "--", 74 + UI_PAD, y + 20, &FreeSans9pt7b, COL_FG);

  pager(page);
  uiPush();
}

void uiWeather(const Place &p, const Weather &w, int page) {
  const unsigned long t = millis();
  canvas.fillScreen(COL_BG);

  if (!w.valid) {
    card(4, 4, 120, 114, "CLIMA");
    iconCloud(64, 60, t, COL_FAINT);
    drawCentered("sem dados", 92, &FreeSans9pt7b, COL_DIM);
    pager(page);
    uiPush();
    return;
  }

  const uint16_t tc = tempColor(w.tempC);
  card(4, 4, 74, 62, nullptr);

  char temp[6];
  snprintf(temp, sizeof(temp), "%.0f", w.tempC);
  draw(temp, 4 + UI_PAD, 46, &FreeSansBold24pt7b, tc);
  canvas.drawCircle(4 + UI_PAD + widthOf(temp, &FreeSansBold24pt7b) + 5, 22, 3, tc);
  micro(w.desc, 4 + UI_PAD, 52, COL_DIM);

  card(82, 4, 42, 32, nullptr);
  weatherIcon(w.code, 103, 20, t);

  int y = card(82, 40, 42, 26, "UMID");
  char hum[6];
  snprintf(hum, sizeof(hum), "%d%%", w.humidity);
  micro(hum, 82 + UI_PAD, y + 1, COL_FG);

  y = card(4, 70, 120, 48, "MIN / MAX");
  char lo[6], hi[6];
  snprintf(lo, sizeof(lo), "%.0f", w.minC);
  snprintf(hi, sizeof(hi), "%.0f", w.maxC);
  draw(lo, 4 + UI_PAD, y + 27, &FreeSans9pt7b, COL_COLD);
  drawRight(hi, 124 - UI_PAD, y + 27, &FreeSans9pt7b, COL_HOT);

  const int bx = 30;
  const int bw = 68;
  canvas.fillRoundRect(bx, y + 12, bw, 4, 2, COL_FAINT);
  const float span = w.maxC - w.minC;
  const float k = span > 0.5f ? (w.tempC - w.minC) / span : 0.5f;
  const int mark = bx + (int)(bw * (k < 0 ? 0 : (k > 1 ? 1 : k)));
  canvas.fillRoundRect(bx, y + 12, mark - bx, 4, 2, tc);
  canvas.fillCircle(mark, y + 14, 3 + ((t / 500) % 2), tc);

  pager(page);
  uiPush();
}

void uiInsight(const char *text, bool pending, int page) {
  const unsigned long t = millis();
  canvas.fillScreen(COL_BG);

  card(4, 4, 120, 76, nullptr);

  if (pending) {
    for (int i = 0; i < 3; ++i) {
      const bool on = ((t / 300) % 3) == (unsigned)i;
      canvas.fillCircle(52 + i * 12, 42, on ? 4 : 2, on ? COL_ACCENT : COL_FAINT);
    }
  } else {
    const int kMaxLines = 4;
    char lines[kMaxLines][24];
    int count = 0;

    const char *p = text;
    while (*p && count < kMaxLines) {
      while (*p == ' ') ++p;
      if (!*p) break;

      char probe[24];
      int take = 0;
      int fit = 0;
      while (p[take]) {
        const int len = take + 1;
        if (len >= (int)sizeof(probe)) break;
        memcpy(probe, p, len);
        probe[len] = '\0';
        if (widthOf(probe, &FreeSans9pt7b) > 100) break;
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
    const int top = 10 + (60 - block > 0 ? (60 - block) / 2 : 0);
    const int wave = (int)(sin(t / 400.0f) * 3.0f);
    canvas.fillRect(10, top + 3 + wave, 2, block - 6, COL_ACCENT);
    for (int i = 0; i < count; ++i) {
      draw(lines[i], 18, top + 14 + i * 15, &FreeSans9pt7b, COL_FG);
    }
  }

  const int y = card(4, 84, 120, 34, nullptr);
  micro("AI", 4 + UI_PAD, y + 5, COL_ACCENT);
  struct tm now;
  if (getLocalTime(&now, 5)) {
    char hhmm[6];
    snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
    microRight(hhmm, 124 - UI_PAD, y + 5, COL_DIM);
  }

  pager(page);
  uiPush();
}

void uiSystem(const Place &p, int page) {
  const unsigned long t = millis();
  canvas.fillScreen(COL_BG);

  int y = card(4, 4, 74, 44, "REDE");
  draw(netSsid(), 4 + UI_PAD, y + 18, &FreeSans9pt7b, COL_FG);

  y = card(82, 4, 42, 44, "SINAL");
  char rssi[8];
  snprintf(rssi, sizeof(rssi), "%d", (int)WiFi.RSSI());
  draw(rssi, 82 + UI_PAD, y + 16, &FreeSansBold9pt7b, COL_ACCENT);
  signalBars(82 + UI_PAD, y + 26, netRssiBars(), t);

  y = card(4, 52, 120, 26, nullptr);
  micro("IP", 4 + UI_PAD, y + 5, COL_DIM);
  microRight(netIp(), 124 - UI_PAD, y + 5, COL_FG);

  y = card(4, 82, 58, 36, "LIVRE");
  char heap[10];
  snprintf(heap, sizeof(heap), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  micro(heap, 4 + UI_PAD, y + 4, COL_GREEN);

  y = card(66, 82, 58, 36, "LIGADO");
  const unsigned long up = millis() / 1000;
  char upt[12];
  snprintf(upt, sizeof(upt), "%luh %02lum", up / 3600, (up % 3600) / 60);
  micro(upt, 66 + UI_PAD, y + 4, COL_FG);

  pager(page);
  uiPush();
}

void uiNight(const struct tm &now, bool timeReady) {
  canvas.fillScreen(COL_BG);
  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
  drawCentered(timeReady ? hhmm : "--:--", 72, &FreeSansBold24pt7b, COL_NIGHT);
  uiPush();
}
