#include "screens.h"

#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#define COL_BG_TOP 0x0B1026
#define COL_BG_BOT 0x1C0B30
#define COL_GLASS 0xFFFFFF
#define COL_LINE 0x3C4A66
#define COL_FG 0xFFFFFF
#define COL_MUTED 0x9FB0CC
#define COL_ACCENT 0x22D3EE
#define COL_VIOLET 0xA78BFA
#define COL_GREEN 0x34D399
#define COL_HOT 0xFB7185
#define COL_COLD 0x60A5FA
#define COL_SUN 0xFBBF24
#define COL_CLOUD 0xB6C4DA
#define COL_RAIN 0x60A5FA
#define COL_NIGHT 0x1B2440

#define FONT_HERO &lv_font_montserrat_28
#define FONT_L &lv_font_montserrat_20
#define FONT_M &lv_font_montserrat_16
#define FONT_S &lv_font_montserrat_14
#define FONT_XS &lv_font_montserrat_12
#define FONT_TAG &lv_font_montserrat_10

#define W 128
#define MARGIN 8
#define INNER (W - MARGIN * 2)

static lv_obj_t *sDash;
static lv_obj_t *sTiles;
static lv_obj_t *sProv;
static lv_obj_t *sNight;
static lv_obj_t *sDots[UI_PAGES];

static lv_obj_t *sLblCity, *sLblTime, *sLblWeekday, *sLblDate, *sBarDay;
static lv_obj_t *sIconBox, *sLblTemp, *sLblDesc, *sQuick[3];
static lv_obj_t *sFcDay[3], *sFcMax[3], *sFcMin[3], *sFcDot[3];
static lv_obj_t *sChart, *sLblSpan, *sLblSun;
static lv_chart_series_t *sSeries;
static lv_obj_t *sLblAqi, *sLblAqiText, *sLblPm25, *sLblUv, *sAqiRing;
static lv_obj_t *sMoonDisc, *sMoonLit, *sLblMoon, *sLblMoonPct;
static lv_obj_t *sLblInsight, *sLblStamp;
static lv_obj_t *sLblSsid, *sLblIp, *sLblRssi, *sLblHeap, *sLblUptime;
static lv_obj_t *sLblProv, *sLblNightTime;

static int sPage;
static int sIconCode = -1000;

static lv_obj_t *text(lv_obj_t *par, const lv_font_t *font, uint32_t color, const char *txt,
                      int cx, int y, int w) {
  lv_obj_t *l = lv_label_create(par);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_label_set_text(l, txt);
  lv_obj_set_size(l, w, lv_font_get_line_height(font) + 2);
  lv_obj_set_pos(l, cx - w / 2, y);
  return l;
}

static lv_obj_t *tagText(lv_obj_t *par, const char *txt, int cx, int y, int w) {
  lv_obj_t *l = text(par, FONT_TAG, COL_MUTED, txt, cx, y, w);
  lv_obj_set_style_text_letter_space(l, 2, 0);
  return l;
}

static lv_obj_t *plate(lv_obj_t *par, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(par);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_bg_color(o, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_bg_opa(o, 28, 0);
  lv_obj_set_style_bg_grad_color(o, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_opa(o, 34, 0);
  lv_obj_set_style_bg_grad_opa(o, 12, 0);
  lv_obj_set_style_radius(o, 14, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_border_opa(o, 60, 0);
  lv_obj_set_style_border_width(o, 1, 0);

  lv_obj_t *gloss = lv_obj_create(o);
  lv_obj_remove_style_all(gloss);
  lv_obj_set_pos(gloss, 8, 1);
  lv_obj_set_size(gloss, w - 16, 1);
  lv_obj_set_style_bg_color(gloss, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_bg_opa(gloss, 90, 0);
  lv_obj_set_style_radius(gloss, 1, 0);
  return o;
}

static void blob(lv_obj_t *par, int cx, int cy, int r, uint32_t color) {
  for (int i = 3; i >= 1; --i) {
    const int rr = r * i / 3;
    lv_obj_t *b = lv_obj_create(par);
    lv_obj_remove_style_all(b);
    lv_obj_set_pos(b, cx - rr, cy - rr);
    lv_obj_set_size(b, rr * 2, rr * 2);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(b, 22, 0);
  }
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

static void rule(lv_obj_t *par, int x, int y, int w, int h) {
  lv_obj_t *l = lv_obj_create(par);
  lv_obj_remove_style_all(l);
  lv_obj_set_pos(l, x, y);
  lv_obj_set_size(l, w, h);
  lv_obj_set_style_bg_color(l, lv_color_hex(COL_LINE), 0);
  lv_obj_set_style_bg_opa(l, LV_OPA_COVER, 0);
}

static lv_obj_t *pageOf(int index) {
  static const uint32_t glow[UI_PAGES] = {COL_VIOLET, COL_SUN,   COL_GREEN,
                                          COL_ACCENT, COL_HOT,   COL_COLD};
  lv_obj_t *tile = lv_tileview_add_tile(sTiles, index, 0, LV_DIR_HOR);
  lv_obj_remove_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(tile, 0, 0);

  blob(tile, index % 2 ? 8 : 118, 16, 46, glow[index]);
  blob(tile, index % 2 ? 116 : 12, 116, 38, COL_ACCENT);
  return tile;
}

static uint32_t codeColor(int code) {
  if (code <= 1) return COL_SUN;
  if (code == 2 || code == 3 || code == 45 || code == 48) return COL_CLOUD;
  if (code >= 95) return COL_HOT;
  if (code >= 71 && code <= 86) return COL_FG;
  return COL_RAIN;
}

static void animScale(void *var, int32_t v) {
  lv_obj_set_style_transform_scale((lv_obj_t *)var, v, 0);
}

static void pulse(lv_obj_t *o, int32_t from, int32_t to, uint32_t time);

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

static void pulse(lv_obj_t *o, int32_t from, int32_t to, uint32_t time) {
  const int32_t w = lv_obj_get_width(o);
  lv_obj_set_style_transform_pivot_x(o, w / 2, 0);
  lv_obj_set_style_transform_pivot_y(o, w / 2, 0);
  animate(o, animScale, from, to, time, 0, true);
}

static void iconSun(lv_obj_t *par, int cx, int cy, int r) {
  lv_obj_t *halo = dot(par, r * 2 + 10, COL_SUN, cx - r - 5, cy - r - 5);
  lv_obj_set_style_bg_opa(halo, LV_OPA_20, 0);
  lv_obj_t *s = dot(par, r * 2, COL_SUN, cx - r, cy - r);
  pulse(s, 236, 276, 2000);
  pulse(halo, 246, 296, 2000);
}

static void iconCloud(lv_obj_t *par, int cx, int cy, uint32_t color) {
  lv_obj_t *g = lv_obj_create(par);
  lv_obj_remove_style_all(g);
  lv_obj_set_pos(g, cx - 17, cy - 9);
  lv_obj_set_size(g, 34, 18);

  dot(g, 13, color, 0, 5);
  dot(g, 17, color, 9, 0);
  dot(g, 12, color, 22, 6);

  lv_obj_t *base = lv_obj_create(g);
  lv_obj_remove_style_all(base);
  lv_obj_set_pos(base, 0, 9);
  lv_obj_set_size(base, 34, 9);
  lv_obj_set_style_radius(base, 4, 0);
  lv_obj_set_style_bg_color(base, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);

  animate(g, (lv_anim_exec_xcb_t)lv_obj_set_x, cx - 19, cx - 15, 3000, 0, true);
}

static void iconDrops(lv_obj_t *par, int cx, int cy, uint32_t color, int size) {
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *d = dot(par, size, color, cx - 10 + i * 9, cy);
    animate(d, (lv_anim_exec_xcb_t)lv_obj_set_y, cy, cy + 12, 950, i * 300, false);
  }
}

static void buildIcon(int code) {
  if (!sIconBox || code == sIconCode) return;
  sIconCode = code;
  lv_obj_clean(sIconBox);

  const int cx = 24;
  if (code <= 1) {
    iconSun(sIconBox, cx, 24, 12);
  } else if (code == 2) {
    iconSun(sIconBox, cx + 9, 13, 8);
    iconCloud(sIconBox, cx - 3, 31, COL_CLOUD);
  } else if (code == 3 || code == 45 || code == 48) {
    iconCloud(sIconBox, cx, 24, COL_CLOUD);
  } else if (code >= 95) {
    iconCloud(sIconBox, cx, 17, 0x6B7A90);
    static lv_point_precise_t pts[] = {{4, 0}, {0, 7}, {5, 7}, {1, 15}};
    lv_obj_t *bolt = lv_line_create(sIconBox);
    lv_line_set_points(bolt, pts, 4);
    lv_obj_set_pos(bolt, cx - 2, 29);
    lv_obj_set_style_line_color(bolt, lv_color_hex(COL_SUN), 0);
    lv_obj_set_style_line_width(bolt, 3, 0);
    lv_obj_set_style_line_rounded(bolt, true, 0);
    animate(bolt, (lv_anim_exec_xcb_t)lv_obj_set_style_opa, LV_OPA_30, LV_OPA_COVER, 700, 0,
            true);
  } else if (code >= 71 && code <= 86) {
    iconCloud(sIconBox, cx, 16, COL_CLOUD);
    iconDrops(sIconBox, cx, 31, COL_FG, 4);
  } else {
    iconCloud(sIconBox, cx, 16, COL_CLOUD);
    iconDrops(sIconBox, cx, 31, COL_RAIN, 3);
  }
}

static void buildClock(lv_obj_t *p) {
  sLblCity = tagText(p, "--", W / 2, 12, INNER);
  sLblTime = text(p, FONT_HERO, COL_FG, "--:--", W / 2, 34, INNER);
  sLblWeekday = text(p, FONT_M, COL_ACCENT, "--", W / 2, 72, INNER);
  sLblDate = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 92, INNER);

  rule(p, 34, 110, 60, 1);

  sBarDay = lv_bar_create(p);
  lv_obj_set_pos(sBarDay, 34, 110);
  lv_obj_set_size(sBarDay, 60, 2);
  lv_bar_set_range(sBarDay, 0, 1439);
  lv_obj_set_style_bg_opa(sBarDay, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_bg_color(sBarDay, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_radius(sBarDay, 1, LV_PART_INDICATOR);
}

static void buildWeather(lv_obj_t *p) {
  tagText(p, "CLIMA", W / 2, 8, INNER);

  sIconBox = lv_obj_create(p);
  lv_obj_remove_style_all(sIconBox);
  lv_obj_set_pos(sIconBox, 6, 24);
  lv_obj_set_size(sIconBox, 48, 48);

  sLblTemp = text(p, FONT_HERO, COL_FG, "--", 86, 32, 72);
  sLblDesc = text(p, FONT_XS, COL_MUTED, "--", W / 2, 78, INNER);

  static const uint32_t colors[3] = {COL_COLD, COL_HOT, COL_ACCENT};
  for (int i = 0; i < 3; ++i) {
    sQuick[i] = text(p, FONT_XS, colors[i], "--", 26 + i * 38, 100, 36);
  }
}

static void buildForecast(lv_obj_t *p) {
  tagText(p, "3 DIAS", W / 2, 8, INNER);
  for (int i = 0; i < 3; ++i) {
    const int cx = 24 + i * 40;
    sFcDay[i] = tagText(p, "--", cx, 30, 38);
    sFcDot[i] = dot(p, 8, COL_CLOUD, cx - 4, 48);
    sFcMax[i] = text(p, FONT_S, COL_FG, "--", cx, 64, 38);
    sFcMin[i] = text(p, FONT_XS, COL_MUTED, "--", cx, 84, 38);
    if (i < 2) rule(p, cx + 20, 30, 1, 68);
  }
}

static void buildChart(lv_obj_t *p) {
  tagText(p, "24 HORAS", W / 2, 8, INNER);

  sChart = lv_chart_create(p);
  lv_obj_set_pos(sChart, MARGIN, 26);
  lv_obj_set_size(sChart, INNER, 58);
  lv_chart_set_type(sChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(sChart, 24);
  lv_chart_set_div_line_count(sChart, 3, 0);
  lv_obj_set_style_bg_opa(sChart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(sChart, 0, 0);
  lv_obj_set_style_pad_all(sChart, 2, 0);
  lv_obj_set_style_line_color(sChart, lv_color_hex(COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_line_width(sChart, 2, LV_PART_ITEMS);
  lv_obj_set_style_size(sChart, 0, 0, LV_PART_INDICATOR);
  sSeries = lv_chart_add_series(sChart, lv_color_hex(COL_ACCENT), LV_CHART_AXIS_PRIMARY_Y);

  sLblSpan = text(p, FONT_XS, COL_FG, "--", W / 2, 90, INNER);
  sLblSun = tagText(p, "--", W / 2, 108, INNER);
}

static void buildInsight(lv_obj_t *p) {
  tagText(p, "AI", W / 2, 8, INNER);
  sLblStamp = tagText(p, "--:--", W / 2, 108, INNER);

  lv_obj_t *card = plate(p, MARGIN, 22, INNER, 82);
  sLblInsight = text(card, FONT_XS, COL_FG, "...", INNER / 2, 0, INNER - 8);
  lv_obj_set_height(sLblInsight, 76);
  lv_obj_align(sLblInsight, LV_ALIGN_CENTER, 0, 0);
}

static void buildSystem(lv_obj_t *p) {
  tagText(p, "SISTEMA", W / 2, 8, INNER);

  lv_obj_t *card = plate(p, MARGIN, 22, INNER, 36);
  sLblSsid = text(card, FONT_S, COL_FG, "--", INNER / 2, 5, INNER - 12);
  sLblIp = text(card, FONT_TAG, COL_MUTED, "--", INNER / 2, 22, INNER - 12);

  lv_obj_t *left = plate(p, MARGIN, 62, 54, 38);
  sLblRssi = text(left, FONT_S, COL_ACCENT, "--", 27, 4, 50);
  tagText(left, "SINAL", 27, 22, 50);

  lv_obj_t *rightCard = plate(p, W / 2 + 2, 62, 54, 38);
  sLblHeap = text(rightCard, FONT_S, COL_COLD, "--", 27, 4, 50);
  tagText(rightCard, "LIVRE", 27, 22, 50);

  sLblUptime = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 106, INNER);
}

static void buildDots(lv_obj_t *parent) {
  const int step = 9;
  const int total = (UI_PAGES - 1) * step + 3;
  const int start = (W - total) / 2;
  for (int i = 0; i < UI_PAGES; ++i) {
    sDots[i] = dot(parent, 3, COL_LINE, start + i * step, 121);
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
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(COL_BG_BOT), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(scr, 0, 0);
}

void screensBuild(void) {
  sDash = lv_obj_create(NULL);
  styleScreen(sDash);

  sTiles = lv_tileview_create(sDash);
  lv_obj_set_size(sTiles, W, 128);
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
  tagText(sProv, "WI-FI", W / 2, 24, INNER);
  text(sProv, FONT_L, COL_ACCENT, "Conecte", W / 2, 42, INNER);
  lv_obj_t *hint = text(sProv, FONT_XS, COL_MUTED, "abra o EspTouch\nno celular", W / 2, 72,
                        INNER);
  lv_obj_set_height(hint, 34);
  lv_obj_set_style_text_line_space(hint, 4, 0);
  sLblProv = text(sProv, FONT_TAG, COL_FG, "aguardando", W / 2, 108, INNER);

  sNight = lv_obj_create(NULL);
  styleScreen(sNight);
  sLblNightTime = text(sNight, FONT_HERO, COL_NIGHT, "--:--", W / 2, 48, INNER);

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

  int hh = 0;
  int mm = 0;
  if (sscanf(hhmm, "%d:%d", &hh, &mm) == 2) lv_bar_set_value(sBarDay, hh * 60 + mm, LV_ANIM_ON);
}

void screensCity(const char *city) { lv_label_set_text(sLblCity, city); }

void screensWeather(float tempC, const char *desc, int humidity, float minC, float maxC,
                    int code) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%.0f\u00b0", tempC);
  lv_label_set_text(sLblTemp, buf);
  lv_obj_set_style_text_color(
      sLblTemp, lv_color_hex(tempC >= 25 ? COL_HOT : (tempC <= 14 ? COL_COLD : COL_FG)), 0);
  lv_label_set_text(sLblDesc, desc);

  snprintf(buf, sizeof(buf), "%.0f\u00b0", minC);
  lv_label_set_text(sQuick[0], buf);
  snprintf(buf, sizeof(buf), "%.0f\u00b0", maxC);
  lv_label_set_text(sQuick[1], buf);
  snprintf(buf, sizeof(buf), "%d%%", humidity);
  lv_label_set_text(sQuick[2], buf);

  buildIcon(code);
}

void screensForecast(int slot, const char *day, float minC, float maxC, int code) {
  if (slot < 0 || slot > 2) return;
  char buf[16];
  lv_label_set_text(sFcDay[slot], day);
  snprintf(buf, sizeof(buf), "%.0f\u00b0", maxC);
  lv_label_set_text(sFcMax[slot], buf);
  snprintf(buf, sizeof(buf), "%.0f\u00b0", minC);
  lv_label_set_text(sFcMin[slot], buf);
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
  snprintf(buf, sizeof(buf), "%.0f\u00b0  a  %.0f\u00b0", lo, hi);
  lv_label_set_text(sLblSpan, buf);
}

void screensSun(const char *sunrise, const char *sunset) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%s   %s", sunrise, sunset);
  lv_label_set_text(sLblSun, buf);
}

void screensInsight(const char *txt, bool pending, const char *stamp) {
  lv_label_set_text(sLblInsight, pending ? "pensando..." : txt);
  lv_obj_set_style_text_opa(sLblInsight, pending ? LV_OPA_50 : LV_OPA_COVER, 0);
  if (stamp) lv_label_set_text(sLblStamp, stamp);
}

void screensSystem(const char *ssid, int rssi, const char *ip, unsigned heapKb,
                   const char *uptime) {
  char buf[24];
  lv_label_set_text(sLblSsid, ssid);
  lv_label_set_text(sLblIp, ip);
  snprintf(buf, sizeof(buf), "%d", rssi);
  lv_label_set_text(sLblRssi, buf);
  snprintf(buf, sizeof(buf), "%u", heapKb);
  lv_label_set_text(sLblHeap, buf);
  snprintf(buf, sizeof(buf), "no ar ha %s", uptime);
  lv_label_set_text(sLblUptime, buf);
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
  lv_obj_set_size(s, W, 128);
  lv_obj_set_style_bg_color(s, lv_color_hex(COL_BG_TOP), 0);
  lv_obj_set_style_bg_grad_color(s, lv_color_hex(COL_BG_BOT), 0);
  lv_obj_set_style_bg_grad_dir(s, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_opa(s, LV_OPA_COVER, 0);
  blob(s, 100, 24, 44, COL_VIOLET);
  text(s, FONT_HERO, COL_ACCENT, title, W / 2, 48, INNER);
  text(s, FONT_XS, COL_MUTED, subtitle, W / 2, 84, INNER);
  lv_obj_delete_delayed(s, 1500);
}
