#include "display.h"
#include "hw_config.h"
#include <Arduino.h>

TFT_eSPI tft = TFT_eSPI();
static uint16_t scaled[SCREEN_W];
static uint8_t backlight_level = 255;

// Paleta da barra (inspirada no Game Boy / SNES cinza)
static const uint16_t COL_BAR      = 0x2945;
static const uint16_t COL_BAR_EDGE = 0x528A;
static const uint16_t COL_RECESS  = 0x18C3;
static const uint16_t COL_DPAD     = 0x4A69;
static const uint16_t COL_DPAD_HI  = 0x6B6D;
static const uint16_t COL_DPAD_CTR = 0x3186;
static const uint16_t COL_BTN_A    = 0xC986;
static const uint16_t COL_BTN_B    = 0x2D47;
static const uint16_t COL_MENU     = 0xFD20;
static const uint16_t COL_PILL     = 0x39E7;

static void draw_pill(int cx, int cy, int w, int h, uint16_t fill, uint16_t edge, const char* label) {
    int x = cx - w / 2;
    int y = cy - h / 2;
    tft.fillRoundRect(x + 1, y + 1, w, h, h / 2, COL_RECESS);
    tft.fillRoundRect(x, y, w, h, h / 2, fill);
    tft.drawRoundRect(x, y, w, h, h / 2, edge);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, fill);
    tft.drawString(label, cx, cy + 1, 2);
}

static void draw_action_btn(int cx, int cy, int r, uint16_t fill, uint16_t edge, const char* label) {
    tft.fillCircle(cx + 1, cy + 2, r, COL_RECESS);
    tft.fillCircle(cx, cy, r, fill);
    tft.drawCircle(cx, cy, r, edge);
    tft.drawCircle(cx, cy, r - 2, TFT_WHITE);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TFT_WHITE, fill);
    tft.drawString(label, cx, cy + 1, 4);
}

static void draw_dpad(int cx, int cy, int arm) {
    const int t = DPAD_THICK;
    const int gap = 7;

    tft.fillRoundRect(cx - t / 2, cy - arm, t, arm - gap, 4, COL_DPAD);
    tft.fillRoundRect(cx - t / 2, cy + gap, t, arm - gap, 4, COL_DPAD);
    tft.fillRoundRect(cx - arm, cy - t / 2, arm - gap, t, 4, COL_DPAD);
    tft.fillRoundRect(cx + gap, cy - t / 2, arm - gap, t, 4, COL_DPAD);
    tft.fillCircle(cx, cy, 6, COL_DPAD_CTR);
    tft.drawCircle(cx, cy, 6, COL_DPAD_HI);

    tft.drawRoundRect(cx - t / 2, cy - arm, t, arm - gap, 4, COL_DPAD_HI);
    tft.drawRoundRect(cx - t / 2, cy + gap, t, arm - gap, 4, COL_DPAD_HI);
    tft.drawRoundRect(cx - arm, cy - t / 2, arm - gap, t, 4, COL_DPAD_HI);
    tft.drawRoundRect(cx + gap, cy - t / 2, arm - gap, t, 4, COL_DPAD_HI);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0xFFE0, COL_DPAD);
    tft.drawString("^", cx, cy - arm + (arm - gap) / 2, 2);
    tft.drawString("v", cx, cy + gap + (arm - gap) / 2, 2);
    tft.drawString("<", cx - arm + gap + (arm - gap) / 2, cy, 2);
    tft.drawString(">", cx + gap + (arm - gap) / 2, cy, 2);
}

void display_init() {
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);
    ledcSetup(0, 5000, 8);
    ledcAttachPin(TFT_PIN_BL, 0);
    ledcWrite(0, 255);
    Serial.printf("[TFT] %dx%d rot=%d OK\n", tft.width(), tft.height(), TFT_ROTATION);
}

void display_set_backlight(uint8_t level) {
    backlight_level = level;
    ledcWrite(0, level);
}

uint8_t display_get_backlight() { return backlight_level; }
void display_clear(uint16_t color) { tft.fillScreen(color); }

void display_push_gb_line(uint8_t y, uint16_t* buf) {
    if (y >= GB_SCREEN_H) return;
    for (int x = 0; x < GB_SCREEN_W; x++) {
        int x0 = x * SCREEN_W / GB_SCREEN_W;
        int x1 = (x + 1) * SCREEN_W / GB_SCREEN_W;
        for (int sx = x0; sx < x1 && sx < SCREEN_W; sx++)
            scaled[sx] = buf[x];
    }
    int y0 = y * GAME_H / GB_SCREEN_H;
    int y1 = (y + 1) * GAME_H / GB_SCREEN_H;
    if (y1 == y0) y1 = y0 + 1;

    for (int sy = y0; sy < y1 && sy < GAME_H; sy++)
        tft.pushImage(0, sy, SCREEN_W, 1, scaled);
}

void display_draw_controls() {
    tft.fillRect(0, CTRL_Y, SCREEN_W, CTRL_H, COL_BAR);
    tft.drawFastHLine(0, CTRL_Y, SCREEN_W, TFT_WHITE);
    tft.drawFastHLine(0, CTRL_Y + 1, SCREEN_W, COL_BAR_EDGE);
    tft.drawFastHLine(0, CTRL_DIV_Y, SCREEN_W, COL_RECESS);

    draw_pill(BTN_MENU_X, BTN_MENU_Y, BTN_MENU_W, BTN_MENU_H, COL_MENU, TFT_WHITE, "MENU");
    draw_pill(BTN_SE_X, BTN_SE_Y, BTN_SE_W, BTN_SE_H, COL_PILL, COL_BAR_EDGE, "SEL");
    draw_pill(BTN_ST_X, BTN_ST_Y, BTN_ST_W, BTN_ST_H, COL_PILL, COL_BAR_EDGE, "ST");

    draw_dpad(DPAD_CX, DPAD_CY, DPAD_ARM);
    draw_action_btn(BTN_A_X, BTN_A_Y, BTN_A_R, COL_BTN_A, TFT_WHITE, "A");
    draw_action_btn(BTN_B_X, BTN_B_Y, BTN_B_R, COL_BTN_B, TFT_WHITE, "B");
}
