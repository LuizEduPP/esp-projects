#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

#include "pins.h"

#define COL_BG 0x0000
#define COL_FG 0xFFFF
#define COL_DIM 0x8410
#define COL_ACCENT 0x07E0
#define COL_WARN 0xFD20
#define COL_COLD 0x05FF

static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
static GFXcanvas16 canvas(UI_W, UI_H);

static const char *const kWeekdays[] = {"domingo", "segunda", "terca",  "quarta",
                                        "quinta",  "sexta",   "sabado"};
static const char *const kMonths[] = {"jan", "fev", "mar", "abr", "mai", "jun",
                                      "jul", "ago", "set", "out", "nov", "dez"};

void uiBegin() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_144GREENTAB);
  tft.setRotation(0);
  tft.fillScreen(COL_BG);
  canvas.setTextWrap(false);
}

void uiPush() { tft.drawRGBBitmap(0, 0, canvas.getBuffer(), UI_W, UI_H); }

static void header(const char *title, bool online) {
  canvas.fillScreen(COL_BG);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(4, 4);
  canvas.print(title);
  canvas.fillCircle(UI_W - 7, 7, 2, online ? COL_ACCENT : COL_WARN);
  canvas.drawFastHLine(4, 14, UI_W - 8, COL_DIM);
}

static int wrapText(const char *text, int x, int y, int width, int size, uint16_t color) {
  const int charW = 6 * size;
  const int lineH = 8 * size + 2;
  const int perLine = width / charW;
  if (perLine < 1) return y;

  canvas.setTextSize(size);
  canvas.setTextColor(color);

  const char *p = text;
  while (*p && y < UI_H) {
    while (*p == ' ') ++p;
    if (!*p) break;

    int take = 0;
    int lastSpace = -1;
    while (p[take] && take < perLine) {
      if (p[take] == ' ') lastSpace = take;
      ++take;
    }
    if (p[take] && lastSpace > 0) take = lastSpace;

    canvas.setCursor(x, y);
    for (int i = 0; i < take; ++i) canvas.write(p[i]);
    y += lineH;
    p += take;
  }
  return y;
}

void uiSplash(const char *line1, const char *line2) {
  canvas.fillScreen(COL_BG);
  canvas.setTextSize(2);
  canvas.setTextColor(COL_ACCENT);
  canvas.setCursor(6, 46);
  canvas.print(line1);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(6, 70);
  canvas.print(line2);
  uiPush();
}

void uiMenu(const char *const *items, int count, int selected, bool online) {
  header("DASH", online);
  for (int i = 0; i < count; ++i) {
    const int y = 26 + i * 18;
    const bool on = (i == selected);
    if (on) canvas.fillRect(2, y - 4, UI_W - 4, 16, 0x1082);
    canvas.setTextSize(1);
    canvas.setTextColor(on ? COL_ACCENT : COL_FG);
    canvas.setCursor(8, y);
    canvas.print(on ? ">" : " ");
    canvas.setCursor(20, y);
    canvas.print(items[i]);
  }
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(4, UI_H - 10);
  canvas.print("NAV mover  SEL abrir");
  uiPush();
}

void uiClock(const struct tm &now, bool timeReady, bool online) {
  header("HORA", online);

  char hhmm[6];
  snprintf(hhmm, sizeof(hhmm), "%02d:%02d", now.tm_hour, now.tm_min);
  canvas.setTextSize(3);
  canvas.setTextColor(timeReady ? COL_FG : COL_DIM);
  canvas.setCursor(9, 36);
  canvas.print(hhmm);

  char ss[4];
  snprintf(ss, sizeof(ss), "%02d", now.tm_sec);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_ACCENT);
  canvas.setCursor(101, 52);
  canvas.print(ss);

  canvas.setTextSize(1);
  canvas.setTextColor(COL_FG);
  canvas.setCursor(9, 74);
  canvas.print(kWeekdays[now.tm_wday % 7]);

  char date[24];
  snprintf(date, sizeof(date), "%02d %s %04d", now.tm_mday, kMonths[now.tm_mon % 12],
           now.tm_year + 1900);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(9, 88);
  canvas.print(date);

  if (!timeReady) {
    canvas.setTextColor(COL_WARN);
    canvas.setCursor(9, 108);
    canvas.print("sincronizando...");
  }
  uiPush();
}

void uiWeather(const Weather &w, bool online) {
  header("CLIMA", online);

  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(4, 22);
  canvas.print(DASH_CITY);

  if (!w.valid) {
    canvas.setTextColor(COL_WARN);
    canvas.setCursor(4, 60);
    canvas.print("sem dados");
    canvas.setTextColor(COL_DIM);
    canvas.setCursor(4, 74);
    canvas.print("NAV tenta de novo");
    uiPush();
    return;
  }

  char temp[10];
  snprintf(temp, sizeof(temp), "%.0fC", w.tempC);
  canvas.setTextSize(3);
  canvas.setTextColor(w.tempC >= 24 ? COL_WARN : COL_COLD);
  canvas.setCursor(6, 38);
  canvas.print(temp);

  wrapText(w.desc, 4, 70, UI_W - 8, 1, COL_FG);

  char meta[28];
  snprintf(meta, sizeof(meta), "umid %d%%  vento %.0fkm/h", w.humidity, w.windKph);
  canvas.setTextSize(1);
  canvas.setTextColor(COL_DIM);
  canvas.setCursor(4, UI_H - 12);
  canvas.print(meta);
  uiPush();
}

void uiInsight(const char *text, bool pending, bool online) {
  header("AI", online);
  if (pending) {
    canvas.setTextSize(1);
    canvas.setTextColor(COL_DIM);
    canvas.setCursor(4, 60);
    canvas.print("pensando...");
  } else {
    wrapText(text, 4, 24, UI_W - 8, 1, COL_FG);
    canvas.setTextSize(1);
    canvas.setTextColor(COL_DIM);
    canvas.setCursor(4, UI_H - 10);
    canvas.print("NAV gera  SEL volta");
  }
  uiPush();
}
