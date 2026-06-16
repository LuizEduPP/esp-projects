#include "touch_input.h"
#include "hw_config.h"
#include <Arduino.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <XPT2046_Bitbang.h>

/* CYD oficial: touch em software SPI (bitbang) para coexistir com display + audio.
 * https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/TROUBLESHOOTING.md
 * https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/tree/main/Examples/Basics/8-Buttons
 */
static XPT2046_Bitbang s_ts(TOUCH_PIN_MOSI, TOUCH_PIN_MISO, TOUCH_PIN_CLK, TOUCH_PIN_CS);
static SemaphoreHandle_t s_touch_mtx = nullptr;

static CydTouchCal cal;
static bool use_custom_cal = false;
static volatile int16_t scr_x = 0;
static volatile int16_t scr_y = 0;
static volatile bool pressed = false;
static Preferences prefs;

#define RAW_X_LO  300
#define RAW_X_HI  3900
#define RAW_Y_LO  3700
#define RAW_Y_HI  200

static void map_screen(int16_t rx, int16_t ry, int16_t* ox, int16_t* oy) {
    int16_t x_lo, x_hi, y_lo, y_hi;
    if (use_custom_cal) {
        x_lo = cal.x_min;
        x_hi = cal.x_max;
        y_lo = cal.y_min;
        y_hi = cal.y_max;
    } else {
        x_lo = RAW_Y_LO;
        x_hi = RAW_Y_HI;
        y_lo = RAW_X_LO;
        y_hi = RAW_X_HI;
    }
    *ox = (int16_t)map(ry, x_hi, x_lo, 0, SCREEN_W - 1);
    *oy = (int16_t)map(rx, y_lo, y_hi, 0, SCREEN_H - 1);
    *ox = constrain(*ox, 0, SCREEN_W - 1);
    *oy = constrain(*oy, 0, SCREEN_H - 1);
}

static CydTouchCal default_calibration() {
    return {(int16_t)RAW_Y_LO, (int16_t)RAW_Y_HI, (int16_t)RAW_X_LO, (int16_t)RAW_X_HI};
}

static bool read_point(int16_t* rx, int16_t* ry) {
    if (!s_touch_mtx || xSemaphoreTake(s_touch_mtx, pdMS_TO_TICKS(12)) != pdTRUE)
        return false;

    const TouchPoint p = s_ts.getTouch();
    xSemaphoreGive(s_touch_mtx);

    if (p.zRaw < 100) return false;
    *rx = (int16_t)p.xRaw;
    *ry = (int16_t)p.yRaw;
    return true;
}

void touch_set_calibration(CydTouchCal c) {
    cal = c;
    use_custom_cal = true;
    prefs.begin("cyd-arcade", false);
    prefs.putBool("cal_ok", true);
    prefs.putShort("xmin", cal.x_min);
    prefs.putShort("xmax", cal.x_max);
    prefs.putShort("ymin", cal.y_min);
    prefs.putShort("ymax", cal.y_max);
    prefs.end();
}

void touch_init() {
    if (!s_touch_mtx)
        s_touch_mtx = xSemaphoreCreateMutex();
    s_ts.begin();

    cal = default_calibration();
    use_custom_cal = false;
    prefs.begin("cyd-arcade", true);
    if (prefs.getBool("cal_ok", false)) {
        cal.x_min = prefs.getShort("xmin", cal.x_min);
        cal.x_max = prefs.getShort("xmax", cal.x_max);
        cal.y_min = prefs.getShort("ymin", cal.y_min);
        cal.y_max = prefs.getShort("ymax", cal.y_max);
        use_custom_cal = true;
    }
    prefs.end();

    Serial.printf("[TOUCH] XPT2046_Bitbang MOSI=%d MISO=%d CLK=%d CS=%d cal=%d\n",
                  TOUCH_PIN_MOSI, TOUCH_PIN_MISO, TOUCH_PIN_CLK, TOUCH_PIN_CS, use_custom_cal);
}

void touch_reinit() {
    if (s_touch_mtx)
        xSemaphoreTake(s_touch_mtx, portMAX_DELAY);
    s_ts.begin();
    if (s_touch_mtx)
        xSemaphoreGive(s_touch_mtx);
}

bool touch_read_raw(int16_t* rx, int16_t* ry) {
    return read_point(rx, ry);
}

void touch_poll() {
    int16_t rx, ry;
    if (read_point(&rx, &ry)) {
        map_screen(rx, ry, (int16_t*)&scr_x, (int16_t*)&scr_y);
        pressed = true;
    } else {
        pressed = false;
    }
}

void touch_wait_release() {
    for (int i = 0; i < 200; i++) {
        touch_poll();
        if (!touch_is_pressed()) return;
        delay(5);
    }
}

void touch_clear_calibration() {
    use_custom_cal = false;
    prefs.begin("cyd-arcade", false);
    prefs.putBool("cal_ok", false);
    prefs.end();
    cal = default_calibration();
    Serial.println("[TOUCH] calibracao apagada");
}

bool touch_is_pressed() {
    return pressed;
}

int16_t touch_get_x() {
    return scr_x;
}

int16_t touch_get_y() {
    return scr_y;
}

bool touch_in_play_area() {
    return pressed && scr_y >= PLAY_Y;
}

int16_t touch_play_x() {
    return (int16_t)(scr_x - PLAY_X);
}

int16_t touch_play_y() {
    return (int16_t)(scr_y - PLAY_Y);
}

bool touch_has_saved_calibration() {
    return use_custom_cal;
}
