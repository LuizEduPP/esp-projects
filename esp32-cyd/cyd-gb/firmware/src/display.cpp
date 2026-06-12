#include "display.h"
#include "hw_config.h"
#include "emulator_bridge.h"
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
    int lw = BTN_UTIL_R + 4;
    if (is_start) {
        int s = BTN_UTIL_R / 2 + 1;
        tft.fillTriangle(cx - s + 1, cy - s, cx - s + 1, cy + s, cx + s + 1, cy, TH->text_lo);
    } else {
        tft.fillRect(cx - lw / 2, cy - 3, lw, 2, TH->text_lo);
        tft.fillRect(cx - lw / 2, cy + 1, lw, 2, TH->text_lo);
    }
}

static void draw_pause_btn() {
    tft.fillRect(BTN_PAUSE_L, BTN_PAUSE_T, BTN_PAUSE_W, BTN_PAUSE_H, TH->pause);
    tft.drawRect(BTN_PAUSE_L, BTN_PAUSE_T, BTN_PAUSE_W, BTN_PAUSE_H, TH->border);
    int cx = BTN_PAUSE_CX;
    int cy = BTN_PAUSE_CY;
    tft.fillRect(cx - 6, cy - 6, 3, 12, TH->text_lo);
    tft.fillRect(cx + 3, cy - 6, 3, 12, TH->text_lo);
}

void display_draw_pause_btn() {
    draw_pause_btn();
}

static void draw_action_btn(int x, int y, int w, int h, uint16_t fill, const char* label) {
    tft.fillRect(x, y, w, h, fill);
    tft.drawRect(x, y, w, h, TH->border);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_lo, fill);
    tft.drawString(label, x + w / 2, y + h / 2, 2);
}

static void draw_analog_stick(int cx, int cy, int16_t stick_dx, int16_t stick_dy, bool active) {
    tft.fillCircle(cx, cy, STICK_BASE_R, TH->stick_base);
    tft.drawCircle(cx, cy, STICK_BASE_R, TH->border);
    tft.drawCircle(cx, cy, STICK_BASE_R - 5, TH->stick_ring);

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
    ui_theme_apply(0);
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
    tft.drawFastHLine(0, STATUS_H - 1, SCREEN_W, TH->accent);

    tft.fillRect(0, BEZEL_Y, SCREEN_W, BEZEL_H, TH->bg);
    tft.drawRoundRect(GAME_X - 1, GAME_Y - 1, GAME_W + 2, GAME_H + 2, 3, TH->border);
    tft.fillRect(GAME_X, GAME_Y, GAME_W, GAME_H, TH->bg);

    draw_pause_btn();
}

void display_draw_status_bar(const char* title, uint32_t fps) {
    if (title) {
        strncpy(status_title, title, 27);
        status_title[27] = 0;
    }
    tft.fillRect(0, 0, SCREEN_W, STATUS_H, TH->surface);
    tft.drawFastHLine(0, STATUS_H - 1, SCREEN_W, TH->accent);
    tft.fillCircle(10, STATUS_H / 2, 3, TH->accent);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TH->text_hi, TH->surface);
    tft.drawString(status_title, 20, STATUS_H / 2, 2);
    display_update_status_fps(fps);
    display_draw_pause_btn();
}

void display_update_status_fps(uint32_t fps) {
    char fps_lbl[10];
    snprintf(fps_lbl, sizeof(fps_lbl), "%u", (unsigned)fps);
    int fx = BTN_PAUSE_L - 44;
    tft.fillRect(fx - 8, 4, 40, STATUS_H - 8, TH->surface);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->text_mute, TH->surface);
    tft.drawString(fps_lbl, fx, STATUS_H / 2, 1);
    tft.setTextDatum(MR_DATUM);
    tft.drawString("fps", fx + 18, STATUS_H / 2, 1);
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
    tft.drawFastHLine(0, CTRL_Y, SCREEN_W, TH->border);

    draw_util_btn(BTN_SE_X, BTN_SE_Y, TH->pill, false);
    draw_util_btn(BTN_ST_X, BTN_ST_Y, TH->pill, true);

    draw_analog_stick(STICK_CX, STICK_CY, 0, 0, false);
    draw_action_btn(BTN_B_L, BTN_AB_T, BTN_B_W, BTN_AB_H, TH->btn_b, "B");
    draw_action_btn(BTN_A_L, BTN_AB_T, BTN_A_W, BTN_AB_H, TH->btn_a, "A");
}

void display_update_dpad(uint8_t dirs, int16_t stick_dx, int16_t stick_dy) {
    (void)dirs;
    tft.fillRect(STICK_BOX_X, STICK_BOX_Y, STICK_BOX_W, STICK_BOX_H, TH->surface);
    draw_analog_stick(STICK_CX, STICK_CY, stick_dx, stick_dy, true);
}
