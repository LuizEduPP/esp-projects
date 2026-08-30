#include "ui.h"

#include <lvgl.h>

#include "dash_config.h"
#include "data.h"
#include "net.h"
#include "render.h"
#include "uidoc.h"

#define W 128
#define H 128
#define MENU_ROWS 5

static lv_obj_t *sStage;
static lv_obj_t *sOverlay;
static lv_obj_t *sOverlayText;
static lv_obj_t *sOverlaySub;
static lv_obj_t *sMenu;
static lv_obj_t *sMenuRow[MENU_ROWS];
static lv_obj_t *sBusy;
static lv_obj_t *sProgress;

static bool sNight = false;
static bool sMenuOpen = false;
static int sPage = 0;
static int sMenuIndex = 0;

static lv_obj_t *bare(lv_obj_t *par, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(par);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}

static lv_obj_t *label(lv_obj_t *par, const lv_font_t *font, uint32_t color, int y, int w) {
  lv_obj_t *l = lv_label_create(par);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
  lv_label_set_text(l, "");
  lv_obj_set_size(l, w, lv_font_get_line_height(font) + 2);
  lv_obj_set_pos(l, (W - w) / 2, y);
  return l;
}

static void buildOverlay() {
  sOverlay = bare(lv_screen_active(), 0, 0, W, H);
  lv_obj_set_style_bg_color(sOverlay, lv_color_hex(0x080B14), 0);
  lv_obj_set_style_bg_opa(sOverlay, LV_OPA_COVER, 0);
  sOverlayText = label(sOverlay, &lv_font_montserrat_16, 0xFFFFFF, 46, 116);
  sOverlaySub = label(sOverlay, &lv_font_montserrat_12, 0x9FB0CC, 70, 116);
}

static void buildMenu() {
  sMenu = bare(lv_screen_active(), 0, 0, W, H);
  lv_obj_set_style_bg_color(sMenu, lv_color_hex(0x0A0E1A), 0);
  lv_obj_set_style_bg_opa(sMenu, 245, 0);
  lv_obj_add_flag(sMenu, LV_OBJ_FLAG_HIDDEN);

  lv_obj_t *title = label(sMenu, &lv_font_montserrat_10, 0x8B93A7, 8, 112);
  lv_label_set_text(title, "TELAS");

  for (int i = 0; i < MENU_ROWS; ++i) {
    sMenuRow[i] = label(sMenu, &lv_font_montserrat_14, 0x9FB0CC, 26 + i * 20, 112);
  }
}

static void buildChrome() {
  sProgress = bare(lv_screen_active(), 0, H - 2, 10, 2);
  lv_obj_set_style_bg_color(sProgress, lv_color_hex(0x38BDF8), 0);
  lv_obj_set_style_bg_opa(sProgress, 200, 0);

  sBusy = bare(lv_screen_active(), W - 8, 4, 4, 4);
  lv_obj_set_style_radius(sBusy, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(sBusy, lv_color_hex(0x34D399), 0);
  lv_obj_set_style_bg_opa(sBusy, LV_OPA_COVER, 0);
  lv_obj_add_flag(sBusy, LV_OBJ_FLAG_HIDDEN);
}

void uiBegin() {
  displayBegin();

  sStage = bare(lv_screen_active(), 0, 0, W, H);
  lv_obj_set_style_bg_color(sStage, lv_color_hex(0x080B14), 0);
  lv_obj_set_style_bg_opa(sStage, LV_OPA_COVER, 0);

  buildChrome();
  buildMenu();
  buildOverlay();
}

void uiTask() { displayTask(); }

static lv_obj_t *sQr = nullptr;

static void overlayText() {
  if (sQr) lv_obj_add_flag(sQr, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_style_bg_color(sOverlay, lv_color_hex(0x080B14), 0);
  lv_obj_set_style_text_color(sOverlaySub, lv_color_hex(0x9FB0CC), 0);
  lv_obj_remove_flag(sOverlayText, LV_OBJ_FLAG_HIDDEN);
  lv_obj_set_y(sOverlaySub, 70);
}

void uiSplash(const char *line1, const char *line2) {
  overlayText();
  lv_label_set_text(sOverlayText, line1);
  lv_label_set_text(sOverlaySub, line2);
  lv_obj_remove_flag(sOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(sOverlay);
  displayTask();
}

void uiStatus(const char *text) {
  lv_label_set_text(sOverlaySub, text);
  displayTask();
}

void uiShowProvisioning(bool armed) {
  if (!armed) {
    overlayText();
    lv_label_set_text(sOverlayText, "WiFi");
    lv_label_set_text(sOverlaySub, "conectando");
  } else {
    char payload[112];
    const int len = snprintf(payload, sizeof(payload),
                             "{\"ver\":\"v1\",\"name\":\"%s\",\"pop\":\"%s\",\"transport\":\"ble\"}",
                             netProvName(), DASH_PROV_POP);

    if (!sQr) {
      sQr = lv_qrcode_create(sOverlay);
      lv_qrcode_set_size(sQr, 104);
      lv_qrcode_set_dark_color(sQr, lv_color_hex(0x000000));
      lv_qrcode_set_light_color(sQr, lv_color_hex(0xFFFFFF));
      lv_qrcode_set_quiet_zone(sQr, false);
    }

    lv_qrcode_update(sQr, payload, len);
    lv_obj_set_pos(sQr, 12, 6);
    lv_obj_remove_flag(sQr, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(sOverlay, lv_color_hex(0xFFFFFF), 0);
    lv_obj_add_flag(sOverlayText, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(sOverlaySub, lv_color_hex(0x333333), 0);
    lv_obj_set_y(sOverlaySub, 113);
    lv_label_set_text(sOverlaySub, netProvName());
  }

  lv_obj_remove_flag(sOverlay, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(sOverlay);
}

static bool sSuspended = false;

void uiSuspend() {
  if (sSuspended) return;
  sSuspended = true;
  renderClear();
  lv_obj_clean(sStage);
  lv_timer_handler();
}

void uiResume() {
  if (!sSuspended) return;
  sSuspended = false;
  uiRebuild();
}

void uiShowDash() {
  lv_obj_add_flag(sOverlay, LV_OBJ_FLAG_HIDDEN);
  uiRebuild();
}

static void syncProgress() {
  const int count = uiDocPageCount();
  if (count <= 0) return;
  const int w = W / count;
  lv_obj_set_width(sProgress, w < 4 ? 4 : w);
  lv_obj_set_x(sProgress, sPage * W / count);
}

void uiRebuild() {
  const int count = uiDocPageCount();
  if (count <= 0) {
    lv_label_set_text(sOverlayText, "sem UI");
    lv_label_set_text(sOverlaySub, "aguardando servidor");
    lv_obj_remove_flag(sOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sOverlay);
    return;
  }

  if (sPage >= count) sPage = 0;

  JsonDocument page;
  if (!uiDocLoadPage(sPage, page)) return;

  renderPage(sStage, page.as<JsonObjectConst>());
  syncProgress();
  lv_obj_move_foreground(sProgress);
  lv_obj_move_foreground(sBusy);
  if (sNight) lv_obj_move_foreground(sOverlay);
}

void uiRefreshValues() {
  if (!sMenuOpen) renderRefresh();
}

void uiTurnPage(int delta) {
  const int count = uiDocPageCount();
  if (count <= 0) return;
  sPage = (sPage + delta + count) % count;
  uiRebuild();
}

int uiPage() { return sPage; }

static void paintMenu() {
  const int count = uiDocPageCount();
  const int first = sMenuIndex - MENU_ROWS / 2 < 0 ? 0
                    : (sMenuIndex + MENU_ROWS / 2 >= count ? count - MENU_ROWS
                                                           : sMenuIndex - MENU_ROWS / 2);
  const int start = first < 0 ? 0 : first;

  for (int i = 0; i < MENU_ROWS; ++i) {
    const int index = start + i;
    if (index >= count) {
      lv_label_set_text(sMenuRow[i], "");
      continue;
    }
    char line[28];
    snprintf(line, sizeof(line), "%s%s", index == sMenuIndex ? "> " : "  ",
             uiDocPageTitle(index));
    lv_label_set_text(sMenuRow[i], line);
    lv_obj_set_style_text_color(
        sMenuRow[i], lv_color_hex(index == sMenuIndex ? 0xFFFFFF : 0x6B7280), 0);
  }
}

void uiMenuToggle() {
  sMenuOpen = !sMenuOpen;
  if (sMenuOpen) {
    sMenuIndex = sPage;
    paintMenu();
    lv_obj_remove_flag(sMenu, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sMenu);
  } else {
    lv_obj_add_flag(sMenu, LV_OBJ_FLAG_HIDDEN);
  }
}

bool uiMenuOpen() { return sMenuOpen; }

void uiMenuMove(int delta) {
  const int count = uiDocPageCount();
  if (count <= 0) return;
  sMenuIndex = (sMenuIndex + delta + count) % count;
  paintMenu();
}

void uiMenuSelect() {
  sPage = sMenuIndex;
  uiMenuToggle();
  uiRebuild();
}

void uiSetNight(bool on) {
  if (sNight == on) return;
  sNight = on;

  if (on) {
    lv_label_set_text(sOverlayText, "");
    lv_label_set_text(sOverlaySub, "");
    lv_obj_set_style_bg_color(sOverlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(sOverlay, DASH_NIGHT_DIM_OPA, 0);
    lv_obj_remove_flag(sOverlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(sOverlay);
  } else {
    lv_obj_set_style_bg_color(sOverlay, lv_color_hex(0x080B14), 0);
    lv_obj_set_style_bg_opa(sOverlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(sOverlay, LV_OBJ_FLAG_HIDDEN);
    uiRebuild();
  }
}

bool uiIsNight() { return sNight; }

void uiBusy(bool on) {
  if (on) {
    lv_obj_remove_flag(sBusy, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(sBusy, LV_OBJ_FLAG_HIDDEN);
  }
  displayTask();
}
