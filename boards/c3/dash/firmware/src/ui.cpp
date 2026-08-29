#include "ui.h"

#include <WiFi.h>
#include <lvgl.h>

#define COL_BG 0x05070A
#define COL_CARD_TOP 0x161D28
#define COL_CARD_BOT 0x0D131B
#define COL_BORDER 0x243040
#define COL_FG 0xFFFFFF
#define COL_DIM 0x8A97A8
#define COL_ACCENT 0x22D3EE
#define COL_HOT 0xFB923C
#define COL_COLD 0x60A5FA
#define COL_SUN 0xFACC15
#define COL_CLOUD 0x94A3B8
#define COL_RAIN 0x38BDF8
#define COL_GREEN 0x34D399
#define COL_NIGHT 0x161C24

static lv_obj_t *sDash = nullptr;
static lv_obj_t *sTiles = nullptr;
static lv_obj_t *sProv = nullptr;
static lv_obj_t *sNightScr = nullptr;
static lv_obj_t *sDots[UI_PAGES];

static lv_obj_t *sLblTime, *sBarSec, *sLblWeekday, *sLblDate;
static lv_obj_t *sLblTemp, *sLblDesc, *sLblHum, *sLblMin, *sLblMax, *sBarRange;
static lv_obj_t *sIconBox;
static lv_obj_t *sLblInsight, *sLblInsightTime;
static lv_obj_t *sLblSsid, *sLblRssi, *sLblIp, *sLblHeap, *sLblUp;
static lv_obj_t *sLblProv, *sBarProv;
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
  lv_obj_set_style_radius(c, 10, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(COL_CARD_TOP), 0);
  lv_obj_set_style_bg_grad_color(c, lv_color_hex(COL_CARD_BOT), 0);
  lv_obj_set_style_bg_grad_dir(c, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(COL_BORDER), 0);
  lv_obj_set_style_border_opa(c, LV_OPA_60, 0);
  lv_obj_set_style_pad_all(c, 6, 0);
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
  lv_obj_t *l = makeLabel(parent, &lv_font_montserrat_12, COL_DIM, text);
  lv_obj_set_style_text_opa(l, LV_OPA_70, 0);
  lv_obj_set_style_text_letter_space(l, 1, 0);
  lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, -2);
  return l;
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

static lv_obj_t *makeDot(lv_obj_t *parent, int size, uint32_t color) {
  lv_obj_t *d = lv_obj_create(parent);
  lv_obj_remove_style_all(d);
  lv_obj_set_size(d, size, size);
  lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(d, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
  return d;
}

static void buildSun(lv_obj_t *par, int cx, int cy) {
  lv_obj_t *s = makeDot(par, 18, COL_SUN);
  lv_obj_align(s, LV_ALIGN_CENTER, cx, cy);
  lv_obj_set_style_shadow_color(s, lv_color_hex(COL_SUN), 0);
  lv_obj_set_style_shadow_width(s, 12, 0);
  lv_obj_set_style_shadow_opa(s, LV_OPA_50, 0);
  animate(s, animSize, 16, 22, 1600, 0, true);
}

static void buildCloud(lv_obj_t *par, int cx, int cy, uint32_t color) {
  lv_obj_t *g = lv_obj_create(par);
  lv_obj_remove_style_all(g);
  lv_obj_set_size(g, 34, 18);
  lv_obj_align(g, LV_ALIGN_CENTER, cx, cy);

  lv_obj_t *a = makeDot(g, 14, color);
  lv_obj_align(a, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_t *b = makeDot(g, 20, color);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, 0);
  lv_obj_t *c = makeDot(g, 14, color);
  lv_obj_align(c, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

  lv_obj_t *base = lv_obj_create(g);
  lv_obj_remove_style_all(base);
  lv_obj_set_size(base, 32, 9);
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
    lv_obj_align(d, LV_ALIGN_CENTER, cx - 8 + i * 8, cy);
    animate(d, (lv_anim_exec_xcb_t)lv_obj_set_y, lv_obj_get_y(d), lv_obj_get_y(d) + 14, 900,
            i * 300, false);
  }
}

static void buildIcon(int code) {
  if (!sIconBox || code == sIconCode) return;
  sIconCode = code;
  lv_obj_clean(sIconBox);

  if (code <= 1) {
    buildSun(sIconBox, 0, -6);
  } else if (code == 2) {
    buildSun(sIconBox, 8, -12);
    buildCloud(sIconBox, -4, 0, COL_CLOUD);
  } else if (code == 3 || code == 45 || code == 48) {
    buildCloud(sIconBox, 0, -4, COL_CLOUD);
  } else if (code >= 95) {
    buildCloud(sIconBox, 0, -10, 0x64748B);
    lv_obj_t *bolt = makeDot(sIconBox, 6, COL_SUN);
    lv_obj_set_style_radius(bolt, 2, 0);
    lv_obj_align(bolt, LV_ALIGN_CENTER, 0, 8);
    animate(bolt, animSize, 4, 9, 500, 0, true);
  } else if (code >= 71 && code <= 86) {
    buildCloud(sIconBox, 0, -10, COL_CLOUD);
    buildDrops(sIconBox, 0, 6, COL_FG, 4);
  } else {
    buildCloud(sIconBox, 0, -10, COL_CLOUD);
    buildDrops(sIconBox, 0, 6, COL_RAIN, 3);
  }
}

static void buildClockTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 120, 56);
  sLblTime = makeLabel(c, &lv_font_montserrat_44, COL_FG, "--:--");
  lv_obj_align(sLblTime, LV_ALIGN_CENTER, 0, -4);

  sBarSec = lv_bar_create(c);
  lv_obj_set_size(sBarSec, 96, 3);
  lv_obj_align(sBarSec, LV_ALIGN_BOTTOM_MID, 0, 2);
  lv_bar_set_range(sBarSec, 0, 59);
  lv_obj_set_style_bg_color(sBarSec, lv_color_hex(COL_BORDER), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sBarSec, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_radius(sBarSec, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(sBarSec, 2, LV_PART_INDICATOR);

  lv_obj_t *d = makeCard(tile, 4, 64, 58, 52);
  makeTag(d, "DIA");
  sLblWeekday = makeLabel(d, &lv_font_montserrat_20, COL_ACCENT, "--");
  lv_obj_align(sLblWeekday, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *e = makeCard(tile, 66, 64, 58, 52);
  makeTag(e, "DATA");
  sLblDate = makeLabel(e, &lv_font_montserrat_16, COL_FG, "--");
  lv_obj_align(sLblDate, LV_ALIGN_BOTTOM_LEFT, 0, 2);
}

static void buildWeatherTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 74, 60);
  sLblTemp = makeLabel(c, &lv_font_montserrat_44, COL_FG, "--");
  lv_obj_align(sLblTemp, LV_ALIGN_TOP_LEFT, 0, -8);
  sLblDesc = makeLabel(c, &lv_font_montserrat_12, COL_DIM, "");
  lv_label_set_long_mode(sLblDesc, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sLblDesc, 62);
  lv_obj_align(sLblDesc, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *ic = makeCard(tile, 82, 4, 42, 60);
  sIconBox = lv_obj_create(ic);
  lv_obj_remove_style_all(sIconBox);
  lv_obj_set_size(sIconBox, 40, 38);
  lv_obj_align(sIconBox, LV_ALIGN_TOP_MID, 0, -2);
  sLblHum = makeLabel(ic, &lv_font_montserrat_12, COL_ACCENT, "--%");
  lv_obj_align(sLblHum, LV_ALIGN_BOTTOM_MID, 0, 2);

  lv_obj_t *r = makeCard(tile, 4, 68, 120, 48);
  makeTag(r, "MIN / MAX");
  sBarRange = lv_bar_create(r);
  lv_obj_set_size(sBarRange, 106, 5);
  lv_obj_align(sBarRange, LV_ALIGN_CENTER, 0, 3);
  lv_bar_set_range(sBarRange, 0, 100);
  lv_obj_set_style_bg_color(sBarRange, lv_color_hex(COL_BORDER), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sBarRange, lv_color_hex(COL_COLD), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_color(sBarRange, lv_color_hex(COL_HOT), LV_PART_INDICATOR);
  lv_obj_set_style_bg_grad_dir(sBarRange, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
  lv_obj_set_style_radius(sBarRange, 3, LV_PART_MAIN);
  lv_obj_set_style_radius(sBarRange, 3, LV_PART_INDICATOR);

  sLblMin = makeLabel(r, &lv_font_montserrat_14, COL_COLD, "--");
  lv_obj_align(sLblMin, LV_ALIGN_BOTTOM_LEFT, 0, 2);
  sLblMax = makeLabel(r, &lv_font_montserrat_14, COL_HOT, "--");
  lv_obj_align(sLblMax, LV_ALIGN_BOTTOM_RIGHT, 0, 2);
}

static void buildInsightTile(lv_obj_t *tile) {
  lv_obj_t *c = makeCard(tile, 4, 4, 120, 80);
  sLblInsight = makeLabel(c, &lv_font_montserrat_12, COL_FG, "...");
  lv_label_set_long_mode(sLblInsight, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sLblInsight, 106);
  lv_obj_set_style_text_line_space(sLblInsight, 3, 0);
  lv_obj_align(sLblInsight, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *m = makeCard(tile, 4, 88, 120, 28);
  lv_obj_t *tag = makeLabel(m, &lv_font_montserrat_12, COL_ACCENT, "AI");
  lv_obj_align(tag, LV_ALIGN_LEFT_MID, 0, 0);
  sLblInsightTime = makeLabel(m, &lv_font_montserrat_12, COL_DIM, "--:--");
  lv_obj_align(sLblInsightTime, LV_ALIGN_RIGHT_MID, 0, 0);
}

static void buildSystemTile(lv_obj_t *tile) {
  lv_obj_t *n = makeCard(tile, 4, 4, 74, 44);
  makeTag(n, "REDE");
  sLblSsid = makeLabel(n, &lv_font_montserrat_14, COL_FG, "--");
  lv_label_set_long_mode(sLblSsid, LV_LABEL_LONG_DOT);
  lv_obj_set_width(sLblSsid, 62);
  lv_obj_align(sLblSsid, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *s = makeCard(tile, 82, 4, 42, 44);
  makeTag(s, "SINAL");
  sLblRssi = makeLabel(s, &lv_font_montserrat_14, COL_ACCENT, "--");
  lv_obj_align(sLblRssi, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *i = makeCard(tile, 4, 52, 120, 26);
  lv_obj_t *tag = makeLabel(i, &lv_font_montserrat_12, COL_DIM, "IP");
  lv_obj_set_style_text_opa(tag, LV_OPA_70, 0);
  lv_obj_align(tag, LV_ALIGN_LEFT_MID, 0, 0);
  sLblIp = makeLabel(i, &lv_font_montserrat_12, COL_FG, "--");
  lv_obj_align(sLblIp, LV_ALIGN_RIGHT_MID, 0, 0);

  lv_obj_t *h = makeCard(tile, 4, 82, 58, 34);
  makeTag(h, "LIVRE");
  sLblHeap = makeLabel(h, &lv_font_montserrat_12, COL_GREEN, "--");
  lv_obj_align(sLblHeap, LV_ALIGN_BOTTOM_LEFT, 0, 2);

  lv_obj_t *u = makeCard(tile, 66, 82, 58, 34);
  makeTag(u, "LIGADO");
  sLblUp = makeLabel(u, &lv_font_montserrat_12, COL_FG, "--");
  lv_obj_align(sLblUp, LV_ALIGN_BOTTOM_LEFT, 0, 2);
}

static void buildDots(lv_obj_t *parent) {
  for (int i = 0; i < UI_PAGES; ++i) {
    sDots[i] = makeDot(parent, 4, COL_BORDER);
    lv_obj_align(sDots[i], LV_ALIGN_BOTTOM_MID, -18 + i * 12, -2);
  }
}

static void refreshDots() {
  for (int i = 0; i < UI_PAGES; ++i) {
    const bool on = i == sPage;
    lv_obj_set_style_bg_color(sDots[i], lv_color_hex(on ? COL_ACCENT : COL_BORDER), 0);
    lv_obj_set_size(sDots[i], on ? 10 : 4, 4);
    lv_obj_set_style_radius(sDots[i], 2, 0);
  }
}

static void styleScreen(lv_obj_t *scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

void uiBegin() {
  displayBegin();

  sDash = lv_obj_create(nullptr);
  styleScreen(sDash);

  sTiles = lv_tileview_create(sDash);
  lv_obj_set_size(sTiles, UI_W, UI_H);
  lv_obj_set_style_bg_opa(sTiles, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(sTiles, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(sTiles, LV_SCROLLBAR_MODE_OFF);

  buildClockTile(lv_tileview_add_tile(sTiles, 0, 0, LV_DIR_HOR));
  buildWeatherTile(lv_tileview_add_tile(sTiles, 1, 0, LV_DIR_HOR));
  buildInsightTile(lv_tileview_add_tile(sTiles, 2, 0, LV_DIR_HOR));
  buildSystemTile(lv_tileview_add_tile(sTiles, 3, 0, LV_DIR_HOR));

  buildDots(sDash);
  refreshDots();

  sProv = lv_obj_create(nullptr);
  styleScreen(sProv);
  lv_obj_t *pc = makeCard(sProv, 4, 20, 120, 88);
  lv_obj_t *title = makeLabel(pc, &lv_font_montserrat_20, COL_ACCENT, "Wi-Fi");
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_t *hint = makeLabel(pc, &lv_font_montserrat_12, COL_DIM,
                             "Abra o app EspTouch\ne envie a senha");
  lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 0, 28);
  sLblProv = makeLabel(pc, &lv_font_montserrat_12, COL_FG, "aguardando");
  lv_obj_align(sLblProv, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  sBarProv = lv_bar_create(pc);
  lv_obj_set_size(sBarProv, 106, 3);
  lv_obj_align(sBarProv, LV_ALIGN_BOTTOM_MID, 0, -16);
  lv_bar_set_range(sBarProv, 0, 100);
  lv_obj_set_style_bg_color(sBarProv, lv_color_hex(COL_BORDER), LV_PART_MAIN);
  lv_obj_set_style_bg_color(sBarProv, lv_color_hex(COL_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_radius(sBarProv, 2, LV_PART_MAIN);
  lv_obj_set_style_radius(sBarProv, 2, LV_PART_INDICATOR);
  animate(sBarProv, (lv_anim_exec_xcb_t)lv_bar_set_value, 0, 100, 1400, 0, true);

  sNightScr = lv_obj_create(nullptr);
  styleScreen(sNightScr);
  sLblNightTime = makeLabel(sNightScr, &lv_font_montserrat_44, COL_NIGHT, "--:--");
  lv_obj_center(sLblNightTime);

  lv_screen_load(sDash);
}

void uiTask() { displayTask(); }

void uiSplash(const char *line1, const char *line2) {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *a = makeLabel(scr, &lv_font_montserrat_44, COL_ACCENT, line1);
  lv_obj_align(a, LV_ALIGN_CENTER, 0, -8);
  lv_obj_t *b = makeLabel(scr, &lv_font_montserrat_12, COL_DIM, line2);
  lv_obj_align(b, LV_ALIGN_CENTER, 0, 26);
  lv_timer_handler();
  lv_obj_delete_delayed(a, 1200);
  lv_obj_delete_delayed(b, 1200);
}

void uiShowProvisioning(bool armed) {
  lv_label_set_text(sLblProv, armed ? "aguardando senha" : "conectando");
  if (lv_screen_active() != sProv) lv_screen_load_anim(sProv, LV_SCR_LOAD_ANIM_FADE_IN, 250, 0, false);
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
  if (on) {
    lv_screen_load_anim(sNightScr, LV_SCR_LOAD_ANIM_FADE_IN, 600, 0, false);
  } else {
    lv_screen_load_anim(sDash, LV_SCR_LOAD_ANIM_FADE_IN, 300, 0, false);
  }
}

bool uiIsNight() { return sNight; }

void uiUpdateClock(const struct tm &now, bool timeReady) {
  char buf[16];
  if (timeReady) {
    snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
  } else {
    snprintf(buf, sizeof(buf), "--:--");
  }
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
  lv_obj_set_style_text_color(sLblTemp,
                              lv_color_hex(w.tempC >= 25 ? COL_HOT
                                           : w.tempC <= 14 ? COL_COLD
                                                           : COL_FG),
                              0);
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
