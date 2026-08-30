#include "screens.h"

#include <math.h>
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
static lv_obj_t *sDots[1];

static lv_obj_t *sLblCity, *sLblTime, *sLblWeekday, *sLblDate, *sBarDay;
static lv_obj_t *sIconBox, *sLblTemp, *sLblDesc, *sQuick[3];
static lv_obj_t *sFcDay[3], *sFcMax[3], *sFcMin[3], *sFcDot[3];
static lv_obj_t *sChart, *sLblSpan, *sLblSun;
static lv_chart_series_t *sSeries;
static lv_obj_t *sWindNeedle, *sLblWind, *sLblGust, *sLblPress, *sLblPressTrend;
static lv_obj_t *sRainBar[8], *sLblRain, *sLblRainSub;
static lv_obj_t *sLblRise, *sLblSet, *sLblDaylight, *sSunArc;
static lv_obj_t *sLblCur[3], *sLblCurPct[3];
static lv_obj_t *sLblNews[3], *sLblRate[3];
static lv_obj_t *sLblHolidayDays, *sLblHolidayName, *sLblHolidayDate;
static lv_obj_t *sLblHistYear, *sLblHistText, *sLblPeople, *sLblIss;
static lv_obj_t *sLblCommits, *sLblPushes, *sLblRepo, *sLblWeek, *sLblRepos, *sLblDays;
static lv_obj_t *sBusy;
static lv_obj_t *sLblTimer, *sLblTimerMode, *sTimerArc, *sLblTimerHint;
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

static void marquee(lv_obj_t *l) {
  lv_label_set_long_mode(l, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_anim_duration(l, 6000, 0);
}

static lv_obj_t *wrapBox(lv_obj_t *par, const lv_font_t *font, uint32_t color, int cx, int y,
                         int w, int h) {
  lv_obj_t *box = lv_obj_create(par);
  lv_obj_remove_style_all(box);
  lv_obj_set_pos(box, cx - w / 2, y);
  lv_obj_set_size(box, w, h);

  lv_obj_t *l = lv_label_create(box);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_style_text_line_space(l, 2, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
  lv_label_set_text(l, "--");
  lv_obj_set_width(l, w);
  lv_obj_set_height(l, LV_SIZE_CONTENT);
  lv_obj_set_pos(l, 0, 0);
  return l;
}

static void driftY(void *obj, int32_t v) { lv_obj_set_y((lv_obj_t *)obj, v); }

static void wrapText(lv_obj_t *l, const char *txt) {
  lv_label_set_text(l, txt);
  lv_obj_update_layout(l);

  lv_anim_delete(l, driftY);
  lv_obj_set_y(l, 0);

  const int boxH = lv_obj_get_height(lv_obj_get_parent(l));
  const int textH = lv_obj_get_height(l);
  if (textH <= boxH) return;

  const int travel = textH - boxH;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, l);
  lv_anim_set_exec_cb(&a, driftY);
  lv_anim_set_values(&a, 0, -travel);
  lv_anim_set_duration(&a, travel * 140);
  lv_anim_set_playback_duration(&a, travel * 140);
  lv_anim_set_delay(&a, 2000);
  lv_anim_set_playback_delay(&a, 2000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
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
  static const uint32_t glow[UI_PAGES] = {
      COL_VIOLET, COL_SUN,    COL_GREEN,  COL_ACCENT, COL_RAIN,  COL_ACCENT, COL_GREEN,
      COL_SUN,    0xC7D2FE,   COL_HOT,    COL_GREEN,  COL_VIOLET, COL_SUN,   COL_VIOLET,
      COL_ACCENT, COL_VIOLET, COL_HOT,    COL_HOT,    COL_COLD};
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
  marquee(sLblCity);
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
  marquee(sLblDesc);

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

static lv_obj_t *ring(lv_obj_t *par, int cx, int cy, int d, uint32_t color, int width) {
  lv_obj_t *a = lv_arc_create(par);
  lv_obj_set_pos(a, cx - d / 2, cy - d / 2);
  lv_obj_set_size(a, d, d);
  lv_arc_set_rotation(a, 135);
  lv_arc_set_bg_angles(a, 0, 270);
  lv_arc_set_range(a, 0, 100);
  lv_obj_remove_style(a, NULL, LV_PART_KNOB);
  lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
  lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(a, lv_color_hex(COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
  return a;
}

static void buildWind(lv_obj_t *p) {
  tagText(p, "VENTO", W / 2, 8, INNER);

  lv_obj_t *rose = lv_obj_create(p);
  lv_obj_remove_style_all(rose);
  lv_obj_set_pos(rose, W / 2 - 26, 22);
  lv_obj_set_size(rose, 52, 52);
  lv_obj_set_style_radius(rose, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(rose, lv_color_hex(COL_LINE), 0);
  lv_obj_set_style_border_opa(rose, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(rose, 1, 0);
  lv_obj_set_style_bg_color(rose, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_bg_opa(rose, 16, 0);

  static lv_point_precise_t pts[] = {{0, 0}, {0, 0}, {0, 0}};
  sWindNeedle = lv_line_create(rose);
  lv_line_set_points(sWindNeedle, pts, 3);
  lv_obj_set_style_line_color(sWindNeedle, lv_color_hex(COL_ACCENT), 0);
  lv_obj_set_style_line_width(sWindNeedle, 3, 0);
  lv_obj_set_style_line_rounded(sWindNeedle, true, 0);

  sLblWind = text(p, FONT_S, COL_FG, "--", W / 2, 36, 46);
  sLblGust = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 78, INNER);

  lv_obj_t *strip = plate(p, MARGIN, 94, INNER, 26);
  sLblPress = text(strip, FONT_XS, COL_FG, "--", 46, 5, 76);
  sLblPressTrend = text(strip, FONT_XS, COL_ACCENT, "--", 92, 5, 30);
}

static void buildRain(lv_obj_t *p) {
  tagText(p, "CHUVA 2H", W / 2, 8, INNER);

  sLblRain = text(p, FONT_M, COL_FG, "--", W / 2, 26, INNER);
  sLblRainSub = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 48, INNER);

  for (int i = 0; i < 8; ++i) {
    lv_obj_t *slot = lv_obj_create(p);
    lv_obj_remove_style_all(slot);
    lv_obj_set_pos(slot, MARGIN + i * 14, 68);
    lv_obj_set_size(slot, 11, 38);
    lv_obj_set_style_radius(slot, 3, 0);
    lv_obj_set_style_bg_color(slot, lv_color_hex(COL_GLASS), 0);
    lv_obj_set_style_bg_opa(slot, 20, 0);

    sRainBar[i] = lv_obj_create(slot);
    lv_obj_remove_style_all(sRainBar[i]);
    lv_obj_set_size(sRainBar[i], 11, 2);
    lv_obj_set_pos(sRainBar[i], 0, 36);
    lv_obj_set_style_radius(sRainBar[i], 3, 0);
    lv_obj_set_style_bg_color(sRainBar[i], lv_color_hex(COL_RAIN), 0);
    lv_obj_set_style_bg_opa(sRainBar[i], LV_OPA_COVER, 0);
  }

  text(p, FONT_TAG, COL_MUTED, "agora", 20, 108, 40);
  text(p, FONT_TAG, COL_MUTED, "+2h", 106, 108, 34);
}

static void buildSun(lv_obj_t *p) {
  tagText(p, "SOL", W / 2, 8, INNER);

  sSunArc = ring(p, W / 2, 56, 62, COL_SUN, 5);

  lv_obj_t *disc = dot(p, 18, COL_SUN, W / 2 - 9, 47);
  lv_obj_set_style_bg_opa(disc, 220, 0);
  pulse(disc, 240, 280, 2400);

  sLblDaylight = text(p, FONT_XS, COL_FG, "--", W / 2, 82, INNER);

  lv_obj_t *strip = plate(p, MARGIN, 98, INNER, 24);
  sLblRise = text(strip, FONT_XS, COL_SUN, "--", 28, 4, 52);
  sLblSet = text(strip, FONT_XS, COL_VIOLET, "--", 84, 4, 52);
  rule(strip, 55, 6, 1, 12);
}

static void buildMarket(lv_obj_t *p) {
  tagText(p, "MERCADO", W / 2, 8, INNER);
  static const char *names[3] = {"USD", "EUR", "BTC"};
  for (int i = 0; i < 3; ++i) {
    const int y = 24 + i * 32;
    lv_obj_t *card = plate(p, MARGIN, y, INNER, 28);
    text(card, FONT_TAG, COL_MUTED, names[i], 20, 8, 32);
    rule(card, 40, 5, 1, 16);
    sLblCur[i] = text(card, FONT_S, COL_FG, "--", 72, 4, 54);
    sLblCurPct[i] = text(card, FONT_TAG, COL_GREEN, "--", 105, 8, 34);
  }
}

static void buildDev(lv_obj_t *p) {
  tagText(p, "GITHUB", W / 2, 8, INNER);

  sLblCommits = text(p, FONT_HERO, COL_GREEN, "--", W / 2, 22, INNER);
  sLblPushes = text(p, FONT_TAG, COL_MUTED, "commits hoje", W / 2, 58, INNER);

  lv_obj_t *strip = plate(p, MARGIN, 74, INNER, 28);
  sLblWeek = text(strip, FONT_XS, COL_ACCENT, "--", 20, 6, 34);
  rule(strip, 38, 5, 1, 16);
  sLblRepos = text(strip, FONT_XS, COL_VIOLET, "--", 56, 6, 34);
  rule(strip, 74, 5, 1, 16);
  sLblDays = text(strip, FONT_XS, COL_GREEN, "--", 92, 6, 34);

  sLblRepo = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 106, INNER);
  marquee(sLblRepo);
}

static void buildTimer(lv_obj_t *p) {
  tagText(p, "POMODORO", W / 2, 8, INNER);

  sTimerArc = ring(p, W / 2, 60, 72, COL_HOT, 6);
  sLblTimer = text(p, FONT_L, COL_FG, "25:00", W / 2, 48, 72);
  sLblTimerMode = text(p, FONT_TAG, COL_MUTED, "foco", W / 2, 70, 72);
  sLblTimerHint = text(p, FONT_TAG, COL_ACCENT, "BOOT para iniciar", W / 2, 104, INNER);
}

static void buildNews(lv_obj_t *p) {
  tagText(p, "BRASIL", W / 2, 8, INNER);
  for (int i = 0; i < 3; ++i) {
    const int y = 24 + i * 33;
    sLblNews[i] = wrapBox(p, FONT_TAG, i == 0 ? COL_FG : COL_MUTED, W / 2, y, INNER, 29);
    if (i < 2) rule(p, 34, y + 30, 60, 1);
  }
}

static void buildRates(lv_obj_t *p) {
  tagText(p, "TAXAS", W / 2, 8, INNER);
  static const char *names[3] = {"SELIC", "CDI", "IPCA"};
  static const uint32_t colors[3] = {COL_ACCENT, COL_VIOLET, COL_HOT};
  for (int i = 0; i < 3; ++i) {
    const int y = 26 + i * 30;
    lv_obj_t *card = plate(p, MARGIN, y, INNER, 26);
    text(card, FONT_TAG, COL_MUTED, names[i], 26, 7, 44);
    rule(card, 52, 5, 1, 14);
    sLblRate[i] = text(card, FONT_S, colors[i], "--", 80, 3, 56);
  }
}

static void buildHoliday(lv_obj_t *p) {
  tagText(p, "PROXIMO FERIADO", W / 2, 8, INNER);
  sLblHolidayDays = text(p, FONT_HERO, COL_SUN, "--", W / 2, 26, INNER);
  text(p, FONT_TAG, COL_MUTED, "dias", W / 2, 62, INNER);
  sLblHolidayName = wrapBox(p, FONT_XS, COL_FG, W / 2, 78, INNER, 28);
  sLblHolidayDate = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 108, INNER);
}

static void buildHistory(lv_obj_t *p) {
  tagText(p, "NESTE DIA", W / 2, 8, INNER);
  sLblHistYear = text(p, FONT_L, COL_VIOLET, "--", W / 2, 22, INNER);

  plate(p, MARGIN, 46, INNER, 66);
  sLblHistText = wrapBox(p, FONT_TAG, COL_FG, W / 2, 52, INNER - 12, 54);
}

static void buildSpace(lv_obj_t *p) {
  tagText(p, "ESPACO", W / 2, 8, INNER);
  sLblPeople = text(p, FONT_HERO, COL_ACCENT, "--", W / 2, 26, INNER);
  text(p, FONT_TAG, COL_MUTED, "pessoas em orbita", W / 2, 64, INNER);

  lv_obj_t *card = plate(p, MARGIN, 82, INNER, 30);
  text(card, FONT_TAG, COL_MUTED, "ISS", 24, 9, 32);
  rule(card, 46, 6, 1, 16);
  sLblIss = text(card, FONT_XS, COL_FG, "--", 76, 7, 60);
}

static uint32_t aqiColor(int aqi) {
  if (aqi <= 20) return COL_GREEN;
  if (aqi <= 40) return COL_ACCENT;
  if (aqi <= 60) return COL_SUN;
  if (aqi <= 80) return 0xFB923C;
  return COL_HOT;
}

static void buildAir(lv_obj_t *p) {
  tagText(p, "AR & UV", W / 2, 8, INNER);

  sAqiRing = lv_arc_create(p);
  lv_obj_set_pos(sAqiRing, W / 2 - 30, 22);
  lv_obj_set_size(sAqiRing, 60, 60);
  lv_arc_set_rotation(sAqiRing, 135);
  lv_arc_set_bg_angles(sAqiRing, 0, 270);
  lv_arc_set_range(sAqiRing, 0, 100);
  lv_obj_remove_style(sAqiRing, NULL, LV_PART_KNOB);
  lv_obj_remove_flag(sAqiRing, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_width(sAqiRing, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sAqiRing, 5, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(sAqiRing, lv_color_hex(COL_LINE), LV_PART_MAIN);
  lv_obj_set_style_arc_rounded(sAqiRing, true, LV_PART_INDICATOR);

  sLblAqi = text(p, FONT_L, COL_FG, "--", W / 2, 40, 52);
  sLblAqiText = text(p, FONT_XS, COL_MUTED, "--", W / 2, 86, INNER);

  lv_obj_t *strip = plate(p, MARGIN, 98, INNER, 24);
  sLblPm25 = text(strip, FONT_XS, COL_VIOLET, "--", 28, 4, 52);
  sLblUv = text(strip, FONT_XS, COL_SUN, "--", 84, 4, 52);
  rule(strip, 55, 6, 1, 12);
}

static void buildMoon(lv_obj_t *p) {
  tagText(p, "LUA", W / 2, 8, INNER);

  sMoonDisc = lv_obj_create(p);
  lv_obj_remove_style_all(sMoonDisc);
  lv_obj_set_pos(sMoonDisc, W / 2 - 27, 24);
  lv_obj_set_size(sMoonDisc, 54, 54);
  lv_obj_set_style_radius(sMoonDisc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sMoonDisc, lv_color_hex(0x1E2740), 0);
  lv_obj_set_style_bg_opa(sMoonDisc, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(sMoonDisc, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_border_opa(sMoonDisc, 50, 0);
  lv_obj_set_style_border_width(sMoonDisc, 1, 0);
  lv_obj_set_style_clip_corner(sMoonDisc, true, 0);

  sMoonLit = lv_obj_create(sMoonDisc);
  lv_obj_remove_style_all(sMoonLit);
  lv_obj_set_size(sMoonLit, 54, 54);
  lv_obj_set_pos(sMoonLit, 54, 0);
  lv_obj_set_style_radius(sMoonLit, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sMoonLit, lv_color_hex(0xF2F5FF), 0);
  lv_obj_set_style_bg_opa(sMoonLit, LV_OPA_COVER, 0);

  sLblMoon = text(p, FONT_S, COL_FG, "--", W / 2, 84, INNER);
  sLblMoonPct = text(p, FONT_TAG, COL_MUTED, "--", W / 2, 104, INNER);
}

static void buildInsight(lv_obj_t *p) {
  tagText(p, "AI", W / 2, 8, INNER);
  sLblStamp = tagText(p, "--:--", W / 2, 108, INNER);

  plate(p, MARGIN, 22, INNER, 82);
  sLblInsight = wrapBox(p, FONT_XS, COL_FG, W / 2, 28, INNER - 10, 70);
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
  lv_obj_t *track = lv_obj_create(parent);
  lv_obj_remove_style_all(track);
  lv_obj_set_pos(track, 34, 122);
  lv_obj_set_size(track, 60, 2);
  lv_obj_set_style_radius(track, 1, 0);
  lv_obj_set_style_bg_color(track, lv_color_hex(COL_GLASS), 0);
  lv_obj_set_style_bg_opa(track, 40, 0);

  sDots[0] = lv_obj_create(track);
  lv_obj_remove_style_all(sDots[0]);
  lv_obj_set_size(sDots[0], 60 / UI_PAGES + 4, 2);
  lv_obj_set_style_radius(sDots[0], 1, 0);
  lv_obj_set_style_bg_color(sDots[0], lv_color_hex(COL_ACCENT), 0);
  lv_obj_set_style_bg_opa(sDots[0], LV_OPA_COVER, 0);
}

static void refreshDots(void) {
  const int span = 60 - (60 / UI_PAGES + 4);
  lv_obj_set_x(sDots[0], span * sPage / (UI_PAGES - 1));
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

  buildClock(pageOf(PAGE_CLOCK));
  buildWeather(pageOf(PAGE_WEATHER));
  buildForecast(pageOf(PAGE_FORECAST));
  buildChart(pageOf(PAGE_CHART));
  buildRain(pageOf(PAGE_RAIN));
  buildWind(pageOf(PAGE_WIND));
  buildAir(pageOf(PAGE_AIR));
  buildSun(pageOf(PAGE_SUN));
  buildMoon(pageOf(PAGE_MOON));
  buildNews(pageOf(PAGE_NEWS));
  buildMarket(pageOf(PAGE_MARKET));
  buildRates(pageOf(PAGE_RATES));
  buildHoliday(pageOf(PAGE_HOLIDAY));
  buildHistory(pageOf(PAGE_HISTORY));
  buildSpace(pageOf(PAGE_SPACE));
  buildDev(pageOf(PAGE_DEV));
  buildTimer(pageOf(PAGE_TIMER));
  buildInsight(pageOf(PAGE_AI));
  buildSystem(pageOf(PAGE_SYSTEM));

  buildDots(sDash);
  refreshDots();

  sBusy = dot(sDash, 5, COL_ACCENT, 116, 7);
  pulse(sBusy, 200, 300, 500);
  lv_obj_add_flag(sBusy, LV_OBJ_FLAG_HIDDEN);

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

void screensAir(int aqi, const char *label, float pm25, float pm10, float uv) {
  char buf[24];
  const uint32_t color = aqiColor(aqi);

  snprintf(buf, sizeof(buf), "%d", aqi);
  lv_label_set_text(sLblAqi, buf);
  lv_obj_set_style_text_color(sLblAqi, lv_color_hex(color), 0);
  lv_label_set_text(sLblAqiText, label);

  lv_arc_set_value(sAqiRing, aqi > 100 ? 100 : aqi);
  lv_obj_set_style_arc_color(sAqiRing, lv_color_hex(color), LV_PART_INDICATOR);

  snprintf(buf, sizeof(buf), "PM %.0f", pm25);
  lv_label_set_text(sLblPm25, buf);
  snprintf(buf, sizeof(buf), "UV %.0f", uv);
  lv_label_set_text(sLblUv, buf);
  (void)pm10;
}

void screensMoon(const char *phase, float illum, bool waxing) {
  if (illum < 0) illum = 0;
  if (illum > 1) illum = 1;

  const int d = 54;
  const int offset = (int)((1.0f - illum) * d + 0.5f);
  lv_obj_set_pos(sMoonLit, waxing ? offset : -offset, 0);

  lv_label_set_text(sLblMoon, phase);

  char buf[24];
  snprintf(buf, sizeof(buf), "%.0f%% iluminada", illum * 100.0f);
  lv_label_set_text(sLblMoonPct, buf);
}

void screensWind(float kph, int dir, float gust, float pressure, float delta) {
  static lv_point_precise_t pts[3];
  static const char *kCompass[8] = {"N", "NE", "L", "SE", "S", "SO", "O", "NO"};

  const float rad = (float)((dir + 180) % 360) * 3.14159265f / 180.0f;
  const int cx = 25;
  const int cy = 25;
  const int len = 18;
  const float tipX = cx + sinf(rad) * len;
  const float tipY = cy - cosf(rad) * len;

  pts[0].x = (int32_t)(cx - sinf(rad) * 8);
  pts[0].y = (int32_t)(cy + cosf(rad) * 8);
  pts[1].x = cx;
  pts[1].y = cy;
  pts[2].x = (int32_t)tipX;
  pts[2].y = (int32_t)tipY;
  lv_line_set_points(sWindNeedle, pts, 3);

  char buf[32];
  snprintf(buf, sizeof(buf), "%.0f", kph);
  lv_label_set_text(sLblWind, buf);

  snprintf(buf, sizeof(buf), "%s  km/h   rajada %.0f", kCompass[((dir + 22) % 360) / 45], gust);
  lv_label_set_text(sLblGust, buf);

  snprintf(buf, sizeof(buf), "%.0f hPa", pressure);
  lv_label_set_text(sLblPress, buf);

  if (delta > 0.6f) {
    snprintf(buf, sizeof(buf), "subindo");
    lv_obj_set_style_text_color(sLblPressTrend, lv_color_hex(COL_GREEN), 0);
  } else if (delta < -0.6f) {
    snprintf(buf, sizeof(buf), "caindo");
    lv_obj_set_style_text_color(sLblPressTrend, lv_color_hex(COL_HOT), 0);
  } else {
    snprintf(buf, sizeof(buf), "estavel");
    lv_obj_set_style_text_color(sLblPressTrend, lv_color_hex(COL_MUTED), 0);
  }
  lv_label_set_text(sLblPressTrend, buf);
}

void screensRain(const float *mm, int count, int startsInMin) {
  float peak = 0.4f;
  for (int i = 0; i < count; ++i) {
    if (mm[i] > peak) peak = mm[i];
  }

  for (int i = 0; i < 8; ++i) {
    const float v = i < count ? mm[i] : 0.0f;
    int h = (int)(v / peak * 34.0f + 0.5f);
    if (h < 2) h = 2;
    lv_obj_set_size(sRainBar[i], 11, h);
    lv_obj_set_pos(sRainBar[i], 0, 38 - h);
    lv_obj_set_style_bg_opa(sRainBar[i], v > 0.05f ? LV_OPA_COVER : 90, 0);
    lv_obj_set_style_bg_color(sRainBar[i], lv_color_hex(v > 0.05f ? COL_RAIN : COL_LINE), 0);
  }

  char buf[32];
  if (startsInMin < 0) {
    lv_label_set_text(sLblRain, "sem chuva");
    lv_obj_set_style_text_color(sLblRain, lv_color_hex(COL_GREEN), 0);
    lv_label_set_text(sLblRainSub, "nas proximas 2 horas");
  } else if (startsInMin == 0) {
    lv_label_set_text(sLblRain, "chovendo");
    lv_obj_set_style_text_color(sLblRain, lv_color_hex(COL_RAIN), 0);
    lv_label_set_text(sLblRainSub, "agora");
  } else {
    lv_label_set_text(sLblRain, "vai chover");
    lv_obj_set_style_text_color(sLblRain, lv_color_hex(COL_RAIN), 0);
    snprintf(buf, sizeof(buf), "em cerca de %d min", startsInMin);
    lv_label_set_text(sLblRainSub, buf);
  }
}

void screensSun2(const char *sunrise, const char *sunset, const char *daylight, int progress) {
  lv_label_set_text(sLblRise, sunrise);
  lv_label_set_text(sLblSet, sunset);
  lv_label_set_text(sLblDaylight, daylight);
  lv_arc_set_value(sSunArc, progress < 0 ? 0 : (progress > 100 ? 100 : progress));
}

static void setQuote(lv_obj_t *value, lv_obj_t *pct, float v, float p, int decimals) {
  char buf[24];
  if (decimals == 0) {
    snprintf(buf, sizeof(buf), "%.0f", v);
  } else {
    snprintf(buf, sizeof(buf), "%.2f", v);
  }
  lv_label_set_text(value, buf);

  snprintf(buf, sizeof(buf), "%+.1f%%", p);
  lv_label_set_text(pct, buf);
  lv_obj_set_style_text_color(pct, lv_color_hex(p >= 0 ? COL_GREEN : COL_HOT), 0);
}

void screensMarket(float usd, float usdPct, float eur, float eurPct, float btc, float btcPct) {
  setQuote(sLblCur[0], sLblCurPct[0], usd, usdPct, 2);
  setQuote(sLblCur[1], sLblCurPct[1], eur, eurPct, 2);
  setQuote(sLblCur[2], sLblCurPct[2], btc / 1000.0f, btcPct, 0);
}

void screensDev(int commitsToday, int commitsWeek, int activeDays, int repos, int followers,
                const char *repo) {
  char buf[32];
  snprintf(buf, sizeof(buf), "%d", commitsToday);
  lv_label_set_text(sLblCommits, buf);
  lv_obj_set_style_text_color(
      sLblCommits, lv_color_hex(commitsToday > 0 ? COL_GREEN : COL_MUTED), 0);

  snprintf(buf, sizeof(buf), commitsToday == 1 ? "commit hoje" : "commits hoje");
  lv_label_set_text(sLblPushes, buf);

  snprintf(buf, sizeof(buf), "%d/7d", commitsWeek);
  lv_label_set_text(sLblWeek, buf);

  snprintf(buf, sizeof(buf), "%d rep", repos);
  lv_label_set_text(sLblRepos, buf);

  snprintf(buf, sizeof(buf), "%dd", activeDays);
  lv_label_set_text(sLblDays, buf);

  lv_label_set_text(sLblRepo, repo);
  (void)followers;
}

void screensNews(const char *a, const char *b, const char *c, int count) {
  const char *items[3] = {a, b, c};
  for (int i = 0; i < 3; ++i) {
    wrapText(sLblNews[i], i < count ? items[i] : "--");
  }
}

void screensRates(float selic, float cdi, float ipca) {
  char buf[16];
  const float values[3] = {selic, cdi, ipca};
  for (int i = 0; i < 3; ++i) {
    snprintf(buf, sizeof(buf), "%.2f%%", values[i]);
    lv_label_set_text(sLblRate[i], buf);
  }
}

void screensHoliday(const char *name, const char *date, int daysLeft) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", daysLeft);
  lv_label_set_text(sLblHolidayDays, buf);
  lv_obj_set_style_text_color(sLblHolidayDays,
                              lv_color_hex(daysLeft <= 7 ? COL_GREEN : COL_SUN), 0);
  wrapText(sLblHolidayName, name);

  if (strlen(date) >= 10) {
    snprintf(buf, sizeof(buf), "%.2s/%.2s", date + 8, date + 5);
    lv_label_set_text(sLblHolidayDate, buf);
  }
}

void screensHistory(int year, const char *txt) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", year);
  lv_label_set_text(sLblHistYear, buf);
  wrapText(sLblHistText, txt);
}

void screensSpace(int people, float lat, float lon) {
  char buf[24];
  snprintf(buf, sizeof(buf), "%d", people);
  lv_label_set_text(sLblPeople, buf);
  snprintf(buf, sizeof(buf), "%.0f, %.0f", lat, lon);
  lv_label_set_text(sLblIss, buf);
}

void screensBusy(bool on) {
  if (!sBusy) return;
  if (on) {
    lv_obj_remove_flag(sBusy, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(sBusy, LV_OBJ_FLAG_HIDDEN);
  }
}

void screensTimer(const char *clock, const char *mode, int percent, bool running) {
  lv_label_set_text(sLblTimer, clock);
  lv_label_set_text(sLblTimerMode, mode);
  lv_arc_set_value(sTimerArc, percent < 0 ? 0 : (percent > 100 ? 100 : percent));
  lv_obj_set_style_arc_color(sTimerArc,
                             lv_color_hex(strcmp(mode, "pausa") == 0 ? COL_GREEN : COL_HOT),
                             LV_PART_INDICATOR);
  lv_label_set_text(sLblTimerHint, running ? "BOOT para pausar" : "BOOT para iniciar");
}

void screensInsight(const char *txt, bool pending, const char *stamp) {
  wrapText(sLblInsight, pending ? "pensando..." : txt);
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
