#include "screens.h"

#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#define COL_BG 0x080A0F
#define COL_SURFACE 0x151B27
#define COL_LINE 0x222A38
#define COL_FG 0xFFFFFF
#define COL_MUTED 0x7E8CA3
#define COL_ACCENT 0x00E5A0
#define COL_HOT 0xFF7A5C
#define COL_COLD 0x5AB8FF
#define COL_SUN 0xFFD166
#define COL_CLOUD 0x9AA8BD
#define COL_RAIN 0x5AB8FF
#define COL_NIGHT 0x1A2029

#define FONT_HERO &lv_font_montserrat_28
#define FONT_L &lv_font_montserrat_20
#define FONT_M &lv_font_montserrat_16
#define FONT_S &lv_font_montserrat_14
#define FONT_XS &lv_font_montserrat_12
#define FONT_TAG &lv_font_montserrat_10

#define MARGIN 8
#define RIGHT 120
#define DOTS_Y 120

static lv_obj_t *sDash;
static lv_obj_t *sTiles;
static lv_obj_t *sProv;
static lv_obj_t *sNight;
static lv_obj_t *sDots[UI_PAGES];

static lv_obj_t *sLblTime, *sLblWeekday, *sLblDate, *sBarSec, *sLblCity;
static lv_obj_t *sLblTemp, *sLblUnit, *sLblDesc, *sIconBox;
static lv_obj_t *sStatVal[3];
static lv_obj_t *sFcDay[3], *sFcRange[3], *sFcDot[3];
static lv_obj_t *sChart, *sLblSpan, *sLblSun;
static lv_chart_series_t *sSeries;
static lv_obj_t *sLblInsight, *sLblStamp;
static lv_obj_t *sSysVal[5];
static lv_obj_t *sLblProv, *sLblNightTime;

static int sPage;
static int sIconCode = -1000;

static lv_obj_t *label(lv_obj_t *par, const lv_font_t *font, uint32_t color, const char *txt,
                       int x, int y) {
  lv_obj_t *l = lv_label_create(par);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, txt);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, x, y);
  return l;
}

static lv_obj_t *labelRight(lv_obj_t *par, const lv_font_t *font, uint32_t color,
                            const char *txt, int right, int y) {
  lv_obj_t *l = label(par, font, color, txt, 0, y);
  lv_obj_align(l, LV_ALIGN_TOP_RIGHT, right - 128, y);
  return l;
}

static lv_obj_t *tag(lv_obj_t *par, const char *txt, int x, int y) {
  lv_obj_t *l = label(par, FONT_TAG, COL_MUTED, txt, x, y);
  lv_obj_set_style_text_letter_space(l, 2, 0);
  return l;
}

static lv_obj_t *plate(lv_obj_t *par, int x, int y, int w, int h, uint32_t color) {
  lv_obj_t *o = lv_obj_create(par);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(o, 10, 0);
  return o;
}

static void hairline(lv_obj_t *par, int y) {
  lv_obj_t *l = lv_obj_create(par);
  lv_obj_remove_style_all(l);
  lv_obj_set_pos(l, MARGIN, y);
  lv_obj_set_size(l, RIGHT - MARGIN, 1);
  lv_obj_set_style_bg_color(l, lv_color_hex(COL_LINE), 0);
  lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
}

static lv_obj_t *dot(lv_obj_t *par, int size, uint32_t color, int x, int y) {
  lv_obj_t *d = lv_obj_create(par);
  lv_obj_remove_style_all(d);
  lv_obj_set_pos(d, x, y);
  lv_obj_set_size(d, size, size);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  return d;
}

static lv_obj_t *pageOf(int index) {
  lv_obj_t *tile = lv_tileview_add_tile(sTiles, index, 0, LV_DIR_HOR);
  lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(tile, 0, 0);
  return tile;
}

static uint32_t codeColor(int code) {
  if (code <= 1) return COL_SUN;
  if (code == 2 || code == 3 || code == 45 || code == 48) return COL_CLOUD;
  if (code >= 95) return COL_HOT;
  if (code >= 71 && code <= 86) return COL_FG;
  return COL_RAIN;
}

static void animSize(void *var, int32_t v) {
  lv_obj_t *o = (lv_obj_t *)var;
  const int32_t cx = lv_obj_get_x(o) + lv_obj_get_width(o) / 2;
  const int32_t cy = lv_obj_get_y(o) + lv_obj_get_height(o) / 2;
  lv_obj_set_size(o, v, v);
  lv_obj_set_pos(o, cx - v / 2, cy - v / 2);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
}

static void animate(lv_obj_t *obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to,
                    uint32_t time, uint32_t delay, bool pingpong) {
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, obj);
  lv_anim_set_exec_cb(&a, cb);
  lv_anim_set_values(&a, from, to);
  lv_anim_set_duration(&a, time);
  lv_anim_set_delay(&a, delay);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_path_cb(&a, pingpong ? lv_anim_path_ease_in_out : lv_anim_path_linear);
  if (pingpong) lv_anim_set_playback_duration(&a, time);
  lv_anim_start(&a);
}

static void iconSun(lv_obj_t *par, int cx, int cy, int r) {
  lv_obj_t *halo = dot(par, r * 2 + 8, COL_SUN, cx - r - 4, cy - r - 4);
  lv_obj_set_style_bg_opa(halo, LV_OPA_20, 0);
  lv_obj_t *s = dot(par, r * 2, COL_SUN, cx - r, cy - r);
  animate(s, animSize, r * 2 - 2, r * 2 + 2, 1800, 0, true);
  animate(halo, animSize, r * 2 + 6, r * 2 + 11, 1800, 0, true);
}

static void iconCloud(lv_obj_t *par, int cx, int cy, uint32_t color) {
  lv_obj_t *g = lv_obj_create(par);
  lv_obj_remove_style_all(g);
  lv_obj_set_pos(g, cx - 15, cy - 8);
  lv_obj_set_size(g, 30, 16);

  dot(g, 12, color, 0, 4);
  dot(g, 15, color, 8, 0);
  dot(g, 11, color, 19, 5);

  lv_obj_t *base = lv_obj_create(g);
  lv_obj_remove_style_all(base);
  lv_obj_set_pos(base, 0, 8);
  lv_obj_set_size(base, 30, 8);
  lv_obj_set_style_radius(base, 4, 0);
  lv_obj_set_style_bg_color(base, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);

  animate(g, (lv_anim_exec_xcb_t)lv_obj_set_x, cx - 17, cx - 13, 2800, 0, true);
}

static void iconDrops(lv_obj_t *par, int cx, int cy, uint32_t color, int size) {
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *d = dot(par, size, color, cx - 9 + i * 8, cy);
    animate(d, (lv_anim_exec_xcb_t)lv_obj_set_y, cy, cy + 12, 950, i * 300, false);
  }
}

static void buildIcon(int code) {
  if (!sIconBox || code == sIconCode) return;
  sIconCode = code;
  lv_obj_clean(sIconBox);

  if (code <= 1) {
    iconSun(sIconBox, 22, 22, 11);
  } else if (code == 2) {
    iconSun(sIconBox, 31, 12, 7);
    iconCloud(sIconBox, 18, 30, COL_CLOUD);
  } else if (code == 3 || code == 45 || code == 48) {
    iconCloud(sIconBox, 22, 22, COL_CLOUD);
  } else if (code >= 95) {
    iconCloud(sIconBox, 22, 16, 0x6B7A90);
    lv_obj_t *bolt = dot(sIconBox, 6, COL_SUN, 19, 30);
    animate(bolt, animSize, 5, 9, 520, 0, true);
  } else if (code >= 71 && code <= 86) {
    iconCloud(sIconBox, 22, 15, COL_CLOUD);
    iconDrops(sIconBox, 22, 28, COL_FG, 4);
  } else {
    iconCloud(sIconBox, 22, 15, COL_CLOUD);
    iconDrops(sIconBox, 22, 28, COL_RAIN, 3);
  }
}

static void buildClock(lv_obj_t *p) {
  sLblCity = tag(p, "--", MARGIN, 8);
  sLblTime = label(p, FONT_HERO, COL_FG, "--:--", MARGIN - 1, 26);
  sLblWeekday = label(p, FONT_L, COL_ACCENT, "--", MARGIN, 62);
  sLblDate = label(p, FONT_XS, COL_MUTED, "--", MARGIN, 88);

  sBarSec = lv_bar_create(p);
  lv_obj_set_pos(sBarSec, MARGIN, 108);
  lv_obj_set_size(sBarSec, RIGHT - MARGIN, 3);
  lv_bar_set_range(sBarSec, 0, 59);
  lv_obj_set_style_bg_color(sBarSec, lv_color_hex(COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sBarSec, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_radius(sBarSec, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(sBarSec, 2, LV_PART_INDICATOR);
}

static void buildWeather(lv_obj_t *p) {
  tag(p, "CLIMA", MARGIN, 8);
  sLblTemp = label(p, FONT_HERO, COL_FG, "--", MARGIN - 1, 24);
  sLblUnit = label(p, FONT_S, COL_MUTED, "\u00b0C", MARGIN + 45, 30);

  sIconBox = lv_obj_create(p);
  lv_obj_remove_style_all(sIconBox);
  lv_obj_set_pos(sIconBox, 74, 16);
  lv_obj_set_size(sIconBox, 44, 44);

  sLblDesc = label(p, FONT_XS, COL_MUTED, "--", MARGIN, 62);
  lv_label_set_long_mode(sLblDesc, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sLblDesc, RIGHT - MARGIN);

  plate(p, MARGIN, 80, RIGHT - MARGIN, 32, COL_SURFACE);
  static const char *names[3] = {"MIN", "MAX", "UMID"};
  static const uint32_t colors[3] = {COL_COLD, COL_HOT, COL_ACCENT};
  for (int i = 0; i < 3; ++i) {
    const int x = MARGIN + 8 + i * 36;
    sStatVal[i] = label(p, FONT_S, colors[i], "--", x, 84);
    tag(p, names[i], x, 101);
  }
}

static void buildForecast(lv_obj_t *p) {
  tag(p, "3 DIAS", MARGIN, 8);
  for (int i = 0; i < 3; ++i) {
    const int y = 26 + i * 30;
    sFcDot[i] = dot(p, 7, COL_CLOUD, MARGIN, y + 6);
    sFcDay[i] = label(p, FONT_S, COL_FG, "--", MARGIN + 14, y);
    sFcRange[i] = labelRight(p, FONT_XS, COL_MUTED, "--", RIGHT, y + 3);
    if (i < 2) hairline(p, y + 22);
  }
}

static void buildChart(lv_obj_t *p) {
  tag(p, "24 HORAS", MARGIN, 8);

  sChart = lv_chart_create(p);
  lv_obj_set_pos(sChart, MARGIN, 24);
  lv_obj_set_size(sChart, RIGHT - MARGIN, 56);
  lv_chart_set_type(sChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(sChart, 24);
  lv_chart_set_div_line_count(sChart, 2, 0);
  lv_obj_set_style_bg_opa(sChart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(sChart, 0, 0);
  lv_obj_set_style_pad_all(sChart, 2, 0);
  lv_obj_set_style_line_color(sChart, lv_color_hex(COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_line_width(sChart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(sChart, 0, 0, LV_PART_INDICATOR);
  sSeries = lv_chart_add_series(sChart, lv_color_hex(COL_ACCENT), LV_CHART_AXIS_PRIMARY_Y);

  hairline(p, 88);
  sLblSpan = label(p, FONT_XS, COL_FG, "--", MARGIN, 94);
  sLblSun = labelRight(p, FONT_TAG, COL_SUN, "--", RIGHT, 97);
}

static void buildInsight(lv_obj_t *p) {
  tag(p, "AI", MARGIN, 8);
  sLblStamp = labelRight(p, FONT_TAG, COL_MUTED, "--:--", RIGHT, 8);

  plate(p, MARGIN, 22, RIGHT - MARGIN, 88, COL_SURFACE);
  sLblInsight = label(p, FONT_XS, COL_FG, "...", MARGIN + 7, 27);
  lv_label_set_long_mode(sLblInsight, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sLblInsight, RIGHT - MARGIN - 13);
  lv_obj_set_style_text_line_space(sLblInsight, 2, 0);
}

static void buildSystem(lv_obj_t *p) {
  tag(p, "SISTEMA", MARGIN, 8);
  static const char *names[5] = {"REDE", "SINAL", "IP", "LIVRE", "LIGADO"};
  for (int i = 0; i < 5; ++i) {
    const int y = 26 + i * 18;
    label(p, FONT_TAG, COL_MUTED, names[i], MARGIN, y + 3);
    sSysVal[i] = labelRight(p, FONT_XS, COL_FG, "--", RIGHT, y);
    if (i < 4) hairline(p, y + 15);
  }
}

static void buildDots(lv_obj_t *parent) {
  const int step = 9;
  const int total = (UI_PAGES - 1) * step + 3;
  const int start = (128 - total) / 2;
  for (int i = 0; i < UI_PAGES; ++i) {
    sDots[i] = dot(parent, 3, COL_LINE, start + i * step, DOTS_Y);
  }
}

static void refreshDots(void) {
  for (int i = 0; i < UI_PAGES; ++i) {
    const bool on = i == sPage;
    lv_obj_set_style_bg_color(sDots[i], lv_color_hex(on ? COL_ACCENT : COL_LINE), 0);
    lv_obj_set_size(sDots[i], on ? 8 : 3, 3);
    lv_obj_set_style_radius(sDots[i], 2, 0);
  }
}

static void styleScreen(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(scr, 0, 0);
}

void screensBuild(void) {
  sDash = lv_obj_create(NULL);
  styleScreen(sDash);

  sTiles = lv_tileview_create(sDash);
  lv_obj_set_size(sTiles, 128, 128);
  lv_obj_set_style_bg_opa(sTiles, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(sTiles, LV_SCROLLBAR_MODE_OFF);

  buildClock(pageOf(0));
  buildWeather(pageOf(1));
  buildForecast(pageOf(2));
  buildChart(pageOf(3));
  buildInsight(pageOf(4));
  buildSystem(pageOf(5));

  buildDots(sDash);
  refreshDots();

  sProv = lv_obj_create(NULL);
  styleScreen(sProv);
  tag(sProv, "WI-FI", MARGIN, 20);
  label(sProv, FONT_L, COL_ACCENT, "Conecte", MARGIN, 36);
  lv_obj_t *hint = label(sProv, FONT_XS, COL_MUTED, "app EspTouch\nno celular", MARGIN, 66);
  lv_obj_set_style_text_line_space(hint, 4, 0);
  sLblProv = label(sProv, FONT_TAG, COL_FG, "aguardando", MARGIN, 106);

  sNight = lv_obj_create(NULL);
  styleScreen(sNight);
  sLblNightTime = label(sNight, FONT_HERO, COL_NIGHT, "--:--", 0, 0);
  lv_obj_center(sLblNightTime);

  lv_screen_load(sDash);
}

void screensGoTo(int pageIndex, bool anim) {
  sPage = (pageIndex + UI_PAGES) % UI_PAGES;
  lv_tileview_set_tile_by_index(sTiles, sPage, 0, anim ? LV_ANIM_ON : LV_ANIM_OFF);
  refreshDots();
}

int screensPage(void) { return sPage; }

void screensClock(const char *hhmm, int sec, const char *weekday, const char *date) {
  lv_label_set_text(sLblTime, hhmm);
  lv_label_set_text(sLblNightTime, hhmm);
  lv_label_set_text(sLblWeekday, weekday);
  lv_label_set_text(sLblDate, date);
  lv_bar_set_value(sBarSec, sec, LV_ANIM_ON);
}

void screensCity(const char *city) { lv_label_set_text(sLblCity, city); }

void screensWeather(float tempC, const char *desc, int humidity, float minC, float maxC,
                    int code) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%.0f", tempC);
  lv_label_set_text(sLblTemp, buf);
  lv_obj_set_style_text_color(
      sLblTemp, lv_color_hex(tempC >= 25 ? COL_HOT : (tempC <= 14 ? COL_COLD : COL_FG)), 0);
  lv_obj_update_layout(sLblTemp);
  lv_obj_align(sLblUnit, LV_ALIGN_TOP_LEFT, MARGIN + lv_obj_get_width(sLblTemp) + 3, 31);
  lv_label_set_text(sLblDesc, desc);

  snprintf(buf, sizeof(buf), "%.0f", minC);
  lv_label_set_text(sStatVal[0], buf);
  snprintf(buf, sizeof(buf), "%.0f", maxC);
  lv_label_set_text(sStatVal[1], buf);
  snprintf(buf, sizeof(buf), "%d%%", humidity);
  lv_label_set_text(sStatVal[2], buf);

  buildIcon(code);
}

void screensForecast(int slot, const char *day, float minC, float maxC, int code) {
  if (slot < 0 || slot > 2) return;
  char buf[16];
  lv_label_set_text(sFcDay[slot], day);
  snprintf(buf, sizeof(buf), "%.0f / %.0f", minC, maxC);
  lv_label_set_text(sFcRange[slot], buf);
  lv_obj_set_style_bg_color(sFcDot[slot], lv_color_hex(codeColor(code)), 0);
}

void screensChart(const float *temps, int count) {
  if (count <= 0) return;
  float lo = temps[0];
  float hi = temps[0];
  for (int i = 0; i < count; ++i) {
    if (temps[i] < lo) lo = temps[i];
    if (temps[i] > hi) hi = temps[i];
  }
  lv_chart_set_range(sChart, LV_CHART_AXIS_PRIMARY_Y, (int)lo - 1, (int)hi + 1);
  lv_chart_set_point_count(sChart, count);
  for (int i = 0; i < count; ++i) {
    lv_chart_set_value_by_id(sChart, sSeries, i, (int)(temps[i] + 0.5f));
  }
  lv_chart_refresh(sChart);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.0f a %.0f C", lo, hi);
  lv_label_set_text(sLblSpan, buf);
}

void screensSun(const char *sunrise, const char *sunset) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%s  %s", sunrise, sunset);
  lv_label_set_text(sLblSun, buf);
}

void screensInsight(const char *text, bool pending, const char *stamp) {
  lv_label_set_text(sLblInsight, pending ? "pensando..." : text);
  lv_obj_set_style_text_opa(sLblInsight, pending ? LV_OPA_50 : LV_OPA_COVER, 0);
  if (stamp) lv_label_set_text(sLblStamp, stamp);
}

void screensSystem(const char *ssid, int rssi, const char *ip, unsigned heapKb,
                   const char *uptime) {
  char buf[24];
  lv_label_set_text(sSysVal[0], ssid);
  snprintf(buf, sizeof(buf), "%d dBm", rssi);
  lv_label_set_text(sSysVal[1], buf);
  lv_label_set_text(sSysVal[2], ip);
  snprintf(buf, sizeof(buf), "%u KB", heapKb);
  lv_label_set_text(sSysVal[3], buf);
  lv_label_set_text(sSysVal[4], uptime);
}

void screensShowNight(bool on, const char *hhmm) {
  if (hhmm) lv_label_set_text(sLblNightTime, hhmm);
  lv_screen_load_anim(on ? sNight : sDash, LV_SCR_LOAD_ANIM_FADE_IN, on ? 600 : 300, 0, false);
}

void screensShowProvisioning(bool armed) {
  lv_label_set_text(sLblProv, armed ? "aguardando senha" : "conectando");
  if (lv_screen_active() != sProv)
    lv_screen_load_anim(sProv, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void screensShowDash(void) {
  if (lv_screen_active() != sDash)
    lv_screen_load_anim(sDash, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void screensSplash(const char *title, const char *subtitle) {
  lv_obj_t *s = lv_obj_create(lv_screen_active());
  lv_obj_remove_style_all(s);
  lv_obj_set_size(s, 128, 128);
  lv_obj_set_style_bg_color(s, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  lv_obj_t *a = label(s, FONT_HERO, COL_ACCENT, title, 0, 0);
  lv_obj_align(a, LV_ALIGN_CENTER, 0, -10);
  lv_obj_t *b = label(s, FONT_XS, COL_MUTED, subtitle, 0, 0);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 20);
  lv_obj_delete_delayed(s, 1500);
}
