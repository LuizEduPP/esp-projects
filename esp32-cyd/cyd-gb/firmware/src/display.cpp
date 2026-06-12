#include "display.h"
#include "hw_config.h"
#include "emulator_bridge.h"
#include "ui_icons.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

TFT_eSPI tft = TFT_eSPI();
static uint16_t scaled[GAME_W];
static uint8_t backlight_level = 255;
static char status_title[28] = "CYD-GB";

#define TH ui_theme_get()

static void draw_util_btn(int cx, int cy, uint16_t fill, bool is_start) {
    tft.fillCircle(cx, cy, BTN_UTIL_R, fill);
    tft.drawCircle(cx, cy, BTN_UTIL_R, TH->border);
    int ix = cx - BTN_UTIL_R + 2;
    int iy = cy - BTN_UTIL_R + 2;
    int isz = BTN_UTIL_R * 2 - 4;
    if (is_start)
        ui_icon_draw(ix, iy, isz, UI_ICON_PLAY, TH->icon);
    else
        ui_icon_draw(ix, iy, isz, UI_ICON_SELECT, TH->icon);
}

static void draw_pause_btn() {
    tft.fillRect(BTN_PAUSE_L, BTN_PAUSE_T, BTN_PAUSE_W, BTN_PAUSE_H, TH->pause);
    tft.drawRect(BTN_PAUSE_L, BTN_PAUSE_T, BTN_PAUSE_W, BTN_PAUSE_H, TH->border);
    ui_icon_draw(BTN_PAUSE_L + 14, BTN_PAUSE_T + 2, 20, UI_ICON_PAUSE, TH->icon);
}

void display_draw_pause_btn() {
    draw_pause_btn();
}

static void draw_action_btn(int x, int y, int w, int h, uint16_t fill, uint16_t edge, const char* label) {
    tft.fillRect(x, y, w, h, fill);
    tft.drawRect(x, y, w, h, edge);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_hi, fill);
    tft.drawString(label, x + w / 2, y + h / 2, 2);
}

static void draw_analog_stick(int cx, int cy, int16_t stick_dx, int16_t stick_dy, bool active) {
    tft.fillCircle(cx, cy, STICK_BASE_R, TH->stick_base);
    tft.drawCircle(cx, cy, STICK_BASE_R, TH->border);

    int kx = cx + (active ? stick_dx : 0);
    int ky = cy + (active ? stick_dy : 0);
    uint16_t knob = active ? TH->stick_act : TH->stick_knob;
    tft.fillCircle(kx, ky, STICK_KNOB_R, knob);
    tft.drawCircle(kx, ky, STICK_KNOB_R, TH->border);
}

#define STICK_BOX_X  (STICK_CX - STICK_BASE_R - 4)
#define STICK_BOX_Y  (STICK_CY - STICK_BASE_R - 4)
#define STICK_BOX_W  (STICK_BASE_R * 2 + 8)
#define STICK_BOX_H  (STICK_BASE_R * 2 + 8)

void display_init() {
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);
    delay(50);
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.setSwapBytes(true);

#ifndef ILI9341_GAMMASET
#define ILI9341_GAMMASET 0x26
#endif
    tft.writecommand(ILI9341_GAMMASET);
    tft.writedata(2);
    delay(120);
    tft.writecommand(ILI9341_GAMMASET);
    tft.writedata(1);

    emu_build_palettes();
    emu_set_palette(0);
    tft.fillScreen(TH->bg);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_PIN_BL, 0);
    ledcWrite(0, 255);
    Serial.printf("[TFT] %dx%d rot=%d inv=BGR OK\n", tft.width(), tft.height(), TFT_ROTATION);
}

void display_set_backlight(uint8_t level) {
    backlight_level = level;
    ledcWrite(0, level);
}

uint8_t display_get_backlight() { return backlight_level; }

void display_clear(uint16_t color) {
    if (color == TFT_BLACK) color = TH->bg;
    tft.fillScreen(color);
}

void display_draw_game_frame() {
    tft.fillRect(0, 0, SCREEN_W, STATUS_H, TH->surface);
    tft.fillRect(0, BEZEL_Y, SCREEN_W, BEZEL_H, TH->bg);
    tft.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H, TH->bg);
    draw_pause_btn();
}

void display_draw_status_bar(const char* title, uint32_t fps) {
    if (title) {
        strncpy(status_title, title, 27);
        status_title[27] = 0;
    }
    tft.fillRect(0, 0, SCREEN_W, STATUS_H, TH->surface);
    ui_icon_draw_t(4, 2, 20, UI_ICON_CART);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString(status_title, 26, STATUS_H / 2, 1);
    display_update_status_fps(fps);
    display_draw_pause_btn();
}

void display_update_status_fps(uint32_t fps) {
    char fps_lbl[16];
    snprintf(fps_lbl, sizeof(fps_lbl), "%u fps", (unsigned)fps);
    tft.fillRect(130, 0, 58, STATUS_H, TH->surface);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString(fps_lbl, 155, STATUS_H / 2, 1);
    display_draw_pause_btn();
}

void display_push_gb_line(uint8_t y, uint16_t* buf) {
    if (y >= GB_SCREEN_H) return;
    for (int x = 0; x < GB_SCREEN_W; x++) {
        int x0 = x * GAME_W / GB_SCREEN_W;
        int x1 = (x + 1) * GAME_W / GB_SCREEN_W;
        for (int sx = x0; sx < x1 && sx < GAME_W; sx++)
            scaled[sx] = buf[x];
    }
    int y0 = GAME_Y + y * GAME_H / GB_SCREEN_H;
    int y1 = GAME_Y + (y + 1) * GAME_H / GB_SCREEN_H;
    if (y1 == y0) y1 = y0 + 1;

    for (int sy = y0; sy < y1 && sy < GAME_Y + GAME_H; sy++)
        tft.pushImage(GAME_X, sy, GAME_W, 1, scaled);
}

void display_draw_controls() {
    tft.fillRect(0, CTRL_Y, SCREEN_W, CTRL_H, TH->surface);

    draw_util_btn(BTN_SE_X, BTN_SE_Y, TH->pill, false);
    draw_util_btn(BTN_ST_X, BTN_ST_Y, TH->pill, true);

    draw_analog_stick(STICK_CX, STICK_CY, 0, 0, false);
    draw_action_btn(BTN_B_L, BTN_AB_T, BTN_B_W, BTN_AB_H, TH->btn_b, TH->btn_b_bd, "B");
    draw_action_btn(BTN_A_L, BTN_AB_T, BTN_A_W, BTN_AB_H, TH->btn_a, TH->btn_a_bd, "A");
}

void display_update_dpad(uint8_t dirs, int16_t stick_dx, int16_t stick_dy) {
    (void)dirs;
    tft.fillRect(STICK_BOX_X, STICK_BOX_Y, STICK_BOX_W, STICK_BOX_H, TH->surface);
    draw_analog_stick(STICK_CX, STICK_CY, stick_dx, stick_dy, true);
}
