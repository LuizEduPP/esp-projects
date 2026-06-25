#include "display.h"
#include "game_catalog.h"
#include "hw_config.h"
#include "touch_input.h"
#include "ui_draw.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <Preferences.h>
#include <stdio.h>

TFT_eSPI tft = TFT_eSPI();

#define BL_CH          0
#define BL_FREQ        5000
#define BL_BITS        8
#define BL_MIN         16
#define BL_MAX         255
#define BL_STEP_PCT    10
#define BL_DEFAULT_PCT 78

static uint8_t s_brightness = BL_MIN;
static uint8_t s_brightness_pct = BL_DEFAULT_PCT;
static bool s_bl_ready = false;

static uint8_t brightness_pct_to_pwm(int pct) {
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    return (uint8_t)(BL_MIN + (uint32_t)(BL_MAX - BL_MIN) * (uint32_t)pct / 100);
}

static uint8_t brightness_pwm_to_pct(uint8_t pwm) {
    if (pwm <= BL_MIN) return 0;
    return (uint8_t)((uint32_t)(pwm - BL_MIN) * 100 / (BL_MAX - BL_MIN));
}

static void display_load_brightness() {
    Preferences prefs;
    prefs.begin("cyd-arcade", true);
    if (prefs.isKey("bright_pct")) {
        s_brightness_pct = prefs.getUChar("bright_pct", BL_DEFAULT_PCT);
    } else {
        const uint8_t legacy = prefs.getUChar("bright", brightness_pct_to_pwm(BL_DEFAULT_PCT));
        s_brightness_pct = brightness_pwm_to_pct(legacy);
    }
    prefs.end();
    s_brightness_pct = (uint8_t)(((s_brightness_pct + BL_STEP_PCT / 2) / BL_STEP_PCT) * BL_STEP_PCT);
    if (s_brightness_pct > 100) s_brightness_pct = 100;
    s_brightness = brightness_pct_to_pwm(s_brightness_pct);
}

static void display_apply_brightness() {
    if (!s_bl_ready) {
        ledcSetup(BL_CH, BL_FREQ, BL_BITS);
        ledcAttachPin(TFT_PIN_BL, BL_CH);
        s_bl_ready = true;
    }
    ledcWrite(BL_CH, s_brightness);
}

void display_brightness_init() {
    display_load_brightness();
    if (s_bl_ready)
        display_apply_brightness();
}

uint8_t display_get_brightness() {
    return s_brightness;
}

void display_set_brightness(uint8_t level) {
    s_brightness = constrain(level, BL_MIN, BL_MAX);
    s_brightness_pct = brightness_pwm_to_pct(s_brightness);
    s_brightness_pct = (uint8_t)(((s_brightness_pct + BL_STEP_PCT / 2) / BL_STEP_PCT) * BL_STEP_PCT);
    s_brightness = brightness_pct_to_pwm(s_brightness_pct);
    display_apply_brightness();
    Preferences prefs;
    prefs.begin("cyd-arcade", false);
    prefs.putUChar("bright_pct", s_brightness_pct);
    prefs.end();
}

int display_brightness_percent() {
    return (int)s_brightness_pct;
}

void display_brightness_step(int delta) {
    int v = (int)s_brightness_pct + delta * BL_STEP_PCT;
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    s_brightness_pct = (uint8_t)v;
    s_brightness = brightness_pct_to_pwm(s_brightness_pct);
    display_apply_brightness();
    Preferences prefs;
    prefs.begin("cyd-arcade", false);
    prefs.putUChar("bright_pct", s_brightness_pct);
    prefs.end();
}

void display_brightness_refresh() {
    if (!s_bl_ready) return;
    ledcWrite(BL_CH, s_brightness);
}

void display_init() {
    ui_theme_init();
    pinMode(TFT_PIN_BL, OUTPUT);
    digitalWrite(TFT_PIN_BL, HIGH);
    display_load_brightness();
    delay(50);
    tft.init();
    tft.setRotation(TFT_ROTATION);
    tft.setSwapBytes(true);
    tft.fillScreen(ui_theme_get()->bg);
    display_apply_brightness();
    Serial.printf("[TFT] %dx%d OK brilho=%d\n", tft.width(), tft.height(), s_brightness);
}

void display_splash(const char* title) {
    (void)title;
    char sub[16];
    snprintf(sub, sizeof(sub), "%d jogos", game_catalog_count());
    ui_draw_splash(nullptr, sub);
}

static void splash_progress(int pct, bool holding) {
    const ArcadeTheme* th = ui_theme_get();
    const int w = UI_CONTENT_W * pct / 100;
    tft.fillRoundRect(UI_PAD, SPLASH_BAR_Y, UI_CONTENT_W, 4, 2, th->card);
    if (w > 0) {
        const uint16_t col = holding ? th->danger : th->accent;
        tft.fillRoundRect(UI_PAD, SPLASH_BAR_Y, w, 4, 2, col);
    }
}

bool display_splash_wait(uint32_t duration_ms) {
    const uint32_t t0 = millis();
    bool touched = false;
    int last_pct = -1;

    while (millis() - t0 < duration_ms) {
        touch_poll();
        const bool down = touch_is_pressed();
        if (down) touched = true;
        const int pct = (int)((millis() - t0) * 100 / (int)duration_ms);
        if (pct != last_pct) {
            splash_progress(pct, down);
            last_pct = pct;
        }
        delay(10);
    }

    touch_poll();
    return touched && touch_is_pressed();
}
