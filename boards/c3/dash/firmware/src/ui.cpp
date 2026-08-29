#include "ui.h"

#include <WiFi.h>
#include <lvgl.h>

#define COL_BG 0x04060A
#define COL_CARD_TOP 0x121826
#define COL_CARD_BOT 0x0A0E16
#define COL_BORDER 0x1E2937
#define COL_FG 0xF2F5F9
#define COL_DIM 0x8695AB
#define COL_ACCENT 0x2DD4BF
#define COL_HOT 0xFB7185
#define COL_COLD 0x60A5FA
#define COL_SUN 0xFDE047
#define COL_CLOUD 0x94A3B8
#define COL_RAIN 0x38BDF8
#define COL_GREEN 0x4ADE80
#define COL_NIGHT 0x141A22

#define FONT_XL &lv_font_montserrat_28
#define FONT_L &lv_font_montserrat_20
#define FONT_M &lv_font_montserrat_16
#define FONT_S &lv_font_montserrat_14
#define FONT_XS &lv_font_montserrat_12
#define FONT_TAG &lv_font_montserrat_10

static lv_obj_t *sDash = nullptr;
static lv_obj_t *sTiles = nullptr;
static lv_obj_t *sProv = nullptr;
static lv_obj_t *sNightScr = nullptr;
static lv_obj_t *sDots[UI_PAGES];

static lv_obj_t *sLblTime, *sBarSec, *sLblWeekday, *sLblDate;
static lv_obj_t *sLblTemp, *sLblDesc, *sLblHum, *sLblMin, *sLblMax, *sBarRange;
static lv_obj_t *sIconBox;
static lv_obj_t *sFcDay[3], *sFcRange[3], *sFcDot[3];
static lv_obj_t *sChart, *sLblChartRange;
static lv_chart_series_t *sSeries = nullptr;
static lv_obj_t *sLblSun, *sLblInsight, *sLblInsightTime;
static lv_obj_t *sLblSsid, *sLblRssi, *sLblIp, *sLblHeap, *sLblUp;
static lv_obj_t *sLblProv;
static lv_obj_t *sLblNightTime;

static int sPage = 0;
static bool sNight = false;
static int sIconCode = -1;

static const char *const kWeekdays[] = {"DOM", "SEG", "TER", "QUA", "QUI", "SEX", "SAB"};
static const char *const kMonths[] = {"jan", "fev", "mar", "abr", "mai", "jun",
                                      "jul", "ago", "set", "out", "nov", "dez"};

static lv_obj_t *makeCard(lv_obj_t *parent, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_pos(c, x, y);
  lv_obj_set_size(c, w, h);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_radius(c, 9, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(COL_CARD_TOP), 0);
  lv_obj_set_style_bg_grad_color(c, lv_color_hex(COL_CARD_BOT), 0);
  lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(COL_BORDER), 0);
  lv_obj_set_style_pad_all(c, 5, 0);
  lv_obj_set_style_pad_row(c, 2, 0);
  lv_obj_set_style_pad_column(c, 4, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                        LV_FLEX_ALIGN_START);
  return c;
}

static lv_obj_t *makeLabel(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                           const char *text) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_label_set_text(l, text);
  return l;
}

static lv_obj_t *makeTag(lv_obj_t *parent, const char *text) {
  lv_obj_t *l = makeLabel(parent, FONT_TAG, COL_DIM, text);
  lv_obj_set_style_text_letter_space(l, 1, 0);
  return l;
}

static lv_obj_t *makeRow(lv_obj_t *parent, int height) {
  lv_obj_t *r = lv_obj_create(parent);
  lv_obj_remove_style_all(r);
  lv_obj_set_width(r, LV_PCT(100));
  lv_obj_set_height(r, height);
  lv_obj_set_flex_flow(r, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(r, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  return r;
}

static lv_obj_t *makeDot(lv_obj_t *parent, int size, uint32_t color) {
  lv_obj_t *d = lv_obj_create(parent);
  lv_obj_remove_style_all(d);
  lv_obj_set_size(d, size, size);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  return d;
}

static lv_obj_t *makeBar(lv_obj_t *parent, int w, int h, uint32_t color) {
  lv_obj_t *b = lv_bar_create(parent);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, lv_color_hex(COL_BORDER), LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), LV_PART_INDICATOR);
  lv_obj_set_style_radius(b, h / 2, LV_PART_MAIN);
  lv_obj_set_style_radius(b, h / 2, LV_PART_INDICATOR);
  return b;
}

static uint32_t codeColor(int code) {
  if (code <= 1) return COL_SUN;
  if (code == 2 || code == 3 || code == 45 || code == 48) return COL_CLOUD;
  if (code >= 95) return COL_HOT;
  if (code >= 71 && code <= 86) return COL_FG;
  return COL_RAIN;
}

static void animSize(void *var, int32_t v) {
  lv_obj_set_size((lv_obj_t *)var, v, v);
  lv_obj_set_style_radius((lv_obj_t *)var, LV_RADIUS_CIRCLE, 0);
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

static void buildSun(lv_obj_t *par, int cx, int cy) {
  lv_obj_t *s = makeDot(par, 16, COL_SUN);
  lv_obj_align(s, LV_ALIGN_CENTER, cx, cy);
  lv_obj_set_style_shadow_color(s, lv_color_hex(COL_SUN), 0);
  lv_obj_set_style_shadow_width(s, 10, 0);
  lv_obj_set_style_shadow_opa(s, LV_OPA_40, 0);
  animate(s, animSize, 14, 19, 1600, 0, true);
}

static void buildCloud(lv_obj_t *par, int cx, int cy, uint32_t color) {
  lv_obj_t *g = lv_obj_create(par);
  lv_obj_remove_style_all(g);
  lv_obj_set_size(g, 30, 16);
  lv_obj_align(g, LV_ALIGN_CENTER, cx, cy);

  lv_obj_t *a = makeDot(g, 12, color);
  lv_obj_align(a, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t *b = makeDot(g, 17, color);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *c = makeDot(g, 12, color);
  lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_t *base = lv_obj_create(g);
  lv_obj_remove_style_all(base);
  lv_obj_set_size(base, 28, 8);
  lv_obj_set_style_radius(base, 4, 0);
  lv_obj_set_style_bg_color(base, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(base, LV_OPA_COVER, 0);
  lv_obj_align(base, LV_ALIGN_BOTTOM_MID, 0, 0);

  animate(g, (lv_anim_exec_xcb_t)lv_obj_set_x, lv_obj_get_x(g) - 3, lv_obj_get_x(g) + 3, 2600,
          0, true);
}

static void buildDrops(lv_obj_t *par, int cx, int cy, uint32_t color, int size) {
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *d = makeDot(par, size, color);
    lv_obj_align(d, LV_ALIGN_CENTER, cx - 7 + i * 7, cy);
    animate(d, (lv_anim_exec_xcb_t)lv_obj_set_y, lv_obj_get_y(d), lv_obj_get_y(d) + 12, 900,
            i * 300, false);
  }
}

static void buildIcon(int code) {
  if (!sIconBox || code == sIconCode) return;
  sIconCode = code;
  lv_obj_clean(sIconBox);

  if (code <= 1) {
    buildSun(sIconBox, 0, -2);
  } else if (code == 2) {
    buildSun(sIconBox, 7, -9);
    buildCloud(sIconBox, -4, 2, COL_CLOUD);
  } else if (code == 3 || code == 45 || code == 48) {
    buildCloud(sIconBox, 0, -2, COL_CLOUD);
  } else if (code >= 95) {
    buildCloud(sIconBox, 0, -7, 0x64748B);
    lv_obj_t *bolt = makeDot(sIconBox, 5, COL_SUN);
    lv_obj_align(bolt, LV_ALIGN_CENTER, 0, 8);
    animate(bolt, animSize, 4, 8, 500, 0, true);
  } else if (code >= 71 && code <= 86) {
    buildCloud(sIconBox, 0, -7, COL_CLOUD);
    buildDrops(sIconBox, 0, 6, COL_FG, 4);
  } else {
    buildCloud(sIconBox, 0, -7, COL_CLOUD);
    buildDrops(sIconBox, 0, 6, COL_RAIN, 3);
  }
}

static void buildClockTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 120, 52);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  sLblTime = makeLabel(c, FONT_XL, COL_FG, "--:--");
  sBarSec = makeBar(c, 92, 3, COL_ACCENT);
  lv_bar_set_range(sBarSec, 0, 59);

  lv_obj_t *d = makeCard(tile, 4, 60, 58, 50);
  makeTag(d, "DIA");
  sLblWeekday = makeLabel(d, FONT_L, COL_ACCENT, "--");

  lv_obj_t *e = makeCard(tile, 66, 60, 58, 50);
  makeTag(e, "DATA");
  sLblDate = makeLabel(e, FONT_M, COL_FG, "--");
}

static void buildWeatherTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 74, 58);
  sLblTemp = makeLabel(c, FONT_XL, COL_FG, "--");
  sLblDesc = makeLabel(c, FONT_XS, COL_DIM, "");
  lv_label_set_long_mode(sLblDesc, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sLblDesc, 62);

  lv_obj_t *ic = makeCard(tile, 82, 4, 42, 58);
  lv_obj_set_flex_align(ic, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  sIconBox = lv_obj_create(ic);
  lv_obj_remove_style_all(sIconBox);
  lv_obj_set_size(sIconBox, 32, 28);
  sLblHum = makeLabel(ic, FONT_XS, COL_ACCENT, "--%");

  lv_obj_t *r = makeCard(tile, 4, 66, 120, 44);
  makeTag(r, "MIN / MAX");
  sBarRange = makeBar(r, 108, 4, COL_HOT);
  lv_obj_set_style_bg_color(sBarRange, lv_color_hex(COL_COLD), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(sBarRange, lv_color_hex(COL_HOT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(sBarRange, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_bar_set_range(sBarRange, 0, 100);

  lv_obj_t *row = makeRow(r, 16);
  sLblMin = makeLabel(row, FONT_S, COL_COLD, "--");
  sLblMax = makeLabel(row, FONT_S, COL_HOT, "--");
}

static void buildForecastTile(lv_obj_t *tile) {
  for (int i = 0; i < 3; ++i) {
    lv_obj_t *c = makeCard(tile, 4, 4 + i * 36, 120, 32);
    lv_obj_set_flex_flow(c, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(c, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    sFcDay[i] = makeLabel(c, FONT_S, COL_FG, "--");
    sFcDot[i] = makeDot(c, 8, COL_CLOUD);
    sFcRange[i] = makeLabel(c, FONT_XS, COL_DIM, "-- / --");
  }
}

static void buildChartTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 120, 76);
  makeTag(c, "PROXIMAS 24H");

  sChart = lv_chart_create(c);
  lv_obj_set_size(sChart, 106, 48);
  lv_chart_set_type(sChart, LV_CHART_TYPE_LINE);
  lv_chart_set_point_count(sChart, 24);
  lv_chart_set_div_line_count(sChart, 3, 0);
  lv_obj_set_style_bg_opa(sChart, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(sChart, 0, 0);
  lv_obj_set_style_pad_all(sChart, 0, 0);
  lv_obj_set_style_line_color(sChart, lv_color_hex(COL_BORDER), LV_PART_MAIN);
  lv_obj_set_style_size(sChart, 0, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(sChart, 2, LV_PART_ITEMS);
  sSeries = lv_chart_add_series(sChart, lv_color_hex(COL_ACCENT), LV_CHART_AXIS_PRIMARY_Y);

  lv_obj_t *m = makeCard(tile, 4, 84, 120, 26);
  lv_obj_set_flex_flow(m, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(m, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  sLblChartRange = makeLabel(m, FONT_XS, COL_DIM, "--");
  sLblSun = makeLabel(m, FONT_XS, COL_SUN, "--:-- --:--");
}

static void buildInsightTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 120, 76);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  sLblInsight = makeLabel(c, FONT_XS, COL_FG, "...");
  lv_label_set_long_mode(sLblInsight, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sLblInsight, 108);
  lv_obj_set_style_text_line_space(sLblInsight, 3, 0);

  lv_obj_t *m = makeCard(tile, 4, 84, 120, 26);
  lv_obj_set_flex_flow(m, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(m, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  makeLabel(m, FONT_XS, COL_ACCENT, "AI");
  sLblInsightTime = makeLabel(m, FONT_XS, COL_DIM, "--:--");
}

static void buildSystemTile(lv_obj_t *tile) {
  lv_obj_t *n = makeCard(tile, 4, 4, 74, 40);
  makeTag(n, "REDE");
  sLblSsid = makeLabel(n, FONT_S, COL_FG, "--");
  lv_label_set_long_mode(sLblSsid, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sLblSsid, 62);

  lv_obj_t *s = makeCard(tile, 82, 4, 42, 40);
  makeTag(s, "SINAL");
  sLblRssi = makeLabel(s, FONT_S, COL_ACCENT, "--");

  lv_obj_t *i = makeCard(tile, 4, 48, 120, 24);
  lv_obj_set_flex_flow(i, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(i, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  makeTag(i, "IP");
  sLblIp = makeLabel(i, FONT_XS, COL_FG, "--");

  lv_obj_t *h = makeCard(tile, 4, 76, 58, 34);
  makeTag(h, "LIVRE");
  sLblHeap = makeLabel(h, FONT_XS, COL_GREEN, "--");

  lv_obj_t *u = makeCard(tile, 66, 76, 58, 34);
  makeTag(u, "LIGADO");
  sLblUp = makeLabel(u, FONT_XS, COL_FG, "--");
}

static void buildDots(lv_obj_t *parent) {
  const int step = 10;
  const int start = -(UI_PAGES - 1) * step / 2;
  for (int i = 0; i < UI_PAGES; ++i) {
    sDots[i] = makeDot(parent, 4, COL_BORDER);
    lv_obj_align(sDots[i], LV_ALIGN_BOTTOM_MID, start + i * step, -4);
  }
}

static void refreshDots() {
  for (int i = 0; i < UI_PAGES; ++i) {
    const bool on = i == sPage;
    lv_obj_set_style_bg_color(sDots[i], lv_color_hex(on ? COL_ACCENT : COL_BORDER), 0);
    lv_obj_set_size(sDots[i], on ? 8 : 4, 4);
    lv_obj_set_style_radius(sDots[i], 2, 0);
  }
}

static void styleScreen(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(scr, 0, 0);
}

void uiBegin() {
  displayBegin();

  sDash = lv_obj_create(nullptr);
  styleScreen(sDash);

  sTiles = lv_tileview_create(sDash);
  lv_obj_set_size(sTiles, UI_W, UI_H);
  lv_obj_set_style_bg_opa(sTiles, LV_OPA_TRANSP, 0);
  lv_obj_set_scrollbar_mode(sTiles, LV_SCROLLBAR_MODE_OFF);

  buildClockTile(lv_tileview_add_tile(sTiles, 0, 0, LV_DIR_HOR));
  buildWeatherTile(lv_tileview_add_tile(sTiles, 1, 0, LV_DIR_HOR));
  buildForecastTile(lv_tileview_add_tile(sTiles, 2, 0, LV_DIR_HOR));
  buildChartTile(lv_tileview_add_tile(sTiles, 3, 0, LV_DIR_HOR));
  buildInsightTile(lv_tileview_add_tile(sTiles, 4, 0, LV_DIR_HOR));
  buildSystemTile(lv_tileview_add_tile(sTiles, 5, 0, LV_DIR_HOR));

  buildDots(sDash);
  refreshDots();

  sProv = lv_obj_create(nullptr);
  styleScreen(sProv);
  lv_obj_t *pc = makeCard(sProv, 4, 22, 120, 84);
  makeTag(pc, "WI-FI");
  makeLabel(pc, FONT_L, COL_ACCENT, "Conecte");
  lv_obj_t *hint = makeLabel(pc, FONT_XS, COL_DIM, "app EspTouch\nno celular");
  lv_obj_set_style_text_line_space(hint, 2, 0);
  sLblProv = makeLabel(pc, FONT_XS, COL_FG, "aguardando");

  sNightScr = lv_obj_create(nullptr);
  styleScreen(sNightScr);
  sLblNightTime = makeLabel(sNightScr, FONT_XL, COL_NIGHT, "--:--");
  lv_obj_center(sLblNightTime);

  lv_screen_load(sDash);
}

void uiTask() { displayTask(); }

void uiSplash(const char *line1, const char *line2) {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *box = lv_obj_create(scr);
  lv_obj_remove_style_all(box);
  lv_obj_set_size(box, UI_W, UI_H);
  lv_obj_set_flex_flow(box, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  makeLabel(box, FONT_XL, COL_ACCENT, line1);
  makeLabel(box, FONT_XS, COL_DIM, line2);
  lv_timer_handler();
  lv_obj_delete_delayed(box, 1500);
}

void uiShowProvisioning(bool armed) {
  lv_label_set_text(sLblProv, armed ? "aguardando senha" : "conectando");
  if (lv_screen_active() != sProv)
    lv_screen_load_anim(sProv, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void uiShowDash() {
  if (lv_screen_active() != sDash)
    lv_screen_load_anim(sDash, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
}

void uiTurnPage(int delta) {
  sPage = (sPage + delta + UI_PAGES) % UI_PAGES;
  lv_tileview_set_tile_by_index(sTiles, sPage, 0, LV_ANIM_ON);
  refreshDots();
}

int uiPage() { return sPage; }

void uiSetNight(bool on) {
  if (sNight == on) return;
  sNight = on;
  lv_screen_load_anim(on ? sNightScr : sDash, LV_SCR_LOAD_ANIM_FADE_IN, on ? 600 : 300, 0,
                      false);
}

bool uiIsNight() { return sNight; }

void uiUpdateClock(const struct tm &now, bool timeReady) {
  char buf[16];
  snprintf(buf, sizeof(buf), timeReady ? "%02d:%02d" : "--:--", now.tm_hour, now.tm_min);
  lv_label_set_text(sLblTime, buf);
  lv_label_set_text(sLblNightTime, buf);

  lv_bar_set_value(sBarSec, timeReady ? now.tm_sec : 0, LV_ANIM_ON);
  lv_label_set_text(sLblWeekday, timeReady ? kWeekdays[now.tm_wday % 7] : "--");

  if (timeReady) {
    snprintf(buf, sizeof(buf), "%02d %s", now.tm_mday, kMonths[now.tm_mon % 12]);
    lv_label_set_text(sLblDate, buf);
  }
}

void uiUpdateWeather(const Weather &w) {
  char buf[24];
  if (!w.valid) {
    lv_label_set_text(sLblTemp, "--");
    lv_label_set_text(sLblDesc, "sem dados");
    return;
  }

  snprintf(buf, sizeof(buf), "%.0f", w.tempC);
  lv_label_set_text(sLblTemp, buf);
  lv_obj_set_style_text_color(
      sLblTemp,
      lv_color_hex(w.tempC >= 25 ? COL_HOT : (w.tempC <= 14 ? COL_COLD : COL_FG)), 0);
  lv_label_set_text(sLblDesc, w.desc);

  snprintf(buf, sizeof(buf), "%d%%", w.humidity);
  lv_label_set_text(sLblHum, buf);
  snprintf(buf, sizeof(buf), "%.0f", w.minC);
  lv_label_set_text(sLblMin, buf);
  snprintf(buf, sizeof(buf), "%.0f", w.maxC);
  lv_label_set_text(sLblMax, buf);

  const float span = w.maxC - w.minC;
  float k = span > 0.5f ? (w.tempC - w.minC) / span : 0.5f;
  if (k < 0) k = 0;
  if (k > 1) k = 1;
  lv_bar_set_value(sBarRange, (int)(k * 100), LV_ANIM_ON);

  buildIcon(w.code);

  for (int i = 0; i < 3; ++i) {
    if (i < w.dayCount) {
      const Forecast &f = w.days[i];
      lv_label_set_text(sFcDay[i], f.weekday >= 0 ? kWeekdays[f.weekday] : "--");
      snprintf(buf, sizeof(buf), "%.0f / %.0f", f.minC, f.maxC);
      lv_label_set_text(sFcRange[i], buf);
      lv_obj_set_style_bg_color(sFcDot[i], lv_color_hex(codeColor(f.code)), 0);
    } else {
      lv_label_set_text(sFcDay[i], "--");
      lv_label_set_text(sFcRange[i], "--");
    }
  }

  if (w.hourlyCount > 0) {
    float lo = w.hourly[0];
    float hi = w.hourly[0];
    for (int i = 0; i < w.hourlyCount; ++i) {
      if (w.hourly[i] < lo) lo = w.hourly[i];
      if (w.hourly[i] > hi) hi = w.hourly[i];
    }
    lv_chart_set_range(sChart, LV_CHART_AXIS_PRIMARY_Y, (int)lo - 1, (int)hi + 1);
    lv_chart_set_point_count(sChart, w.hourlyCount);
    for (int i = 0; i < w.hourlyCount; ++i) {
      lv_chart_set_value_by_id(sChart, sSeries, i, (int)(w.hourly[i] + 0.5f));
    }
    lv_chart_refresh(sChart);

    snprintf(buf, sizeof(buf), "%.0f a %.0f C", lo, hi);
    lv_label_set_text(sLblChartRange, buf);
  }

  snprintf(buf, sizeof(buf), "%s  %s", w.sunrise, w.sunset);
  lv_label_set_text(sLblSun, buf);
}

void uiUpdateInsight(const char *text, bool pending) {
  lv_label_set_text(sLblInsight, pending ? "pensando..." : text);
  lv_obj_set_style_text_opa(sLblInsight, pending ? LV_OPA_50 : LV_OPA_COVER, 0);

  struct tm now;
  if (getLocalTime(&now, 5)) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
    lv_label_set_text(sLblInsightTime, buf);
  }
}

void uiUpdateSystem() {
  char buf[24];
  lv_label_set_text(sLblSsid, netSsid());
  snprintf(buf, sizeof(buf), "%d", (int)WiFi.RSSI());
  lv_label_set_text(sLblRssi, buf);
  lv_label_set_text(sLblIp, netIp());
  snprintf(buf, sizeof(buf), "%u KB", (unsigned)(ESP.getFreeHeap() / 1024));
  lv_label_set_text(sLblHeap, buf);
  const unsigned long up = millis() / 1000;
  snprintf(buf, sizeof(buf), "%luh %02lum", up / 3600, (up % 3600) / 60);
  lv_label_set_text(sLblUp, buf);
}
