



#include "touch_input.h"
#include "hw_config.h"
#include "display.h"
#include "sd_manager.h"
#include "i18n.h"
#include "ui_theme.h"

#define TH ui_theme_get()
#include <Arduino.h>
#include <string.h>
#include <XPT2046_Bitbang.h>


static XPT2046_Bitbang ts(TOUCH_PIN_MOSI, TOUCH_PIN_MISO, TOUCH_PIN_CLK, TOUCH_PIN_CS);

static CydTouchCal cal;
static bool use_custom_cal = false;
static volatile uint16_t cur_btns = 0;
static volatile int16_t scr_x = -1, scr_y = -1;
static volatile bool pressed = false;
static volatile uint8_t dpad_vis = 0;
static volatile int16_t dpad_stick_dx = 0, dpad_stick_dy = 0;
static volatile bool dpad_touch = false;
static volatile bool stick_captured = false;
static uint16_t btn_hold = 0;
static uint32_t last_good_ms = 0;
static SemaphoreHandle_t touch_mtx = nullptr;
#define BTN_HOLD_MS 90
#define SAMPLE_N 3
#define ZONE_RELEASE_MS 120
#define ZONE_LATCH 2
#define DPAD_DEAD 7
#define DPAD_AXIS_MIN 6

static int16_t samp_x[SAMPLE_N], samp_y[SAMPLE_N];
static uint8_t samp_n = 0;

struct TouchZone {
    uint16_t latched;
    uint16_t candidate;
    uint8_t latch_cnt;
    uint32_t last_ms;
};

static TouchZone zone_util, zone_dpad, zone_action;


#define RAW_X_LO  300
#define RAW_X_HI  3900
#define RAW_Y_LO  3700
#define RAW_Y_HI  200

static void reset_gesture();

static bool read_point(int16_t* rx, int16_t* ry, int16_t* rz) {
    if (!touch_mtx || xSemaphoreTake(touch_mtx, pdMS_TO_TICKS(12)) != pdTRUE) return false;

    TouchPoint p = ts.getTouch();
    xSemaphoreGive(touch_mtx);


    if (p.zRaw <= 0) return false;

    *rx = (int16_t)p.xRaw;
    *ry = (int16_t)p.yRaw;
    *rz = (int16_t)p.zRaw;
    return true;
}

static void fill_config(CydGbConfig* cfg, uint8_t palette, uint8_t fskip, uint8_t brightness, uint8_t lang) {
    sd_config_defaults(cfg);
    cfg->palette = palette;
    cfg->frame_skip = fskip;
    cfg->brightness = brightness;
    cfg->language = lang;
    cfg->cal_valid = use_custom_cal;
    if (use_custom_cal) {
        cfg->cal_xmin = cal.x_min;
        cfg->cal_xmax = cal.x_max;
        cfg->cal_ymin = cal.y_min;
        cfg->cal_ymax = cal.y_max;
    }
}

static void apply_config(const CydGbConfig* cfg) {
    if (cfg->language < LANG_COUNT) i18n_set_lang(cfg->language);
    if (cfg->cal_valid) {
        cal.x_min = cfg->cal_xmin;
        cal.x_max = cfg->cal_xmax;
        cal.y_min = cfg->cal_ymin;
        cal.y_max = cfg->cal_ymax;
        use_custom_cal = true;
        Serial.printf("[CAL] SD rawY[%d-%d] rawX[%d-%d]\n",
                      cal.x_min, cal.x_max, cal.y_min, cal.y_max);
    }
}

static void save_cal_to_sd() {
    CydGbConfig cfg;
    if (sd_load_config(&cfg)) {
        cfg.cal_valid = true;
        cfg.cal_xmin = cal.x_min;
        cfg.cal_xmax = cal.x_max;
        cfg.cal_ymin = cal.y_min;
        cfg.cal_ymax = cal.y_max;
    } else {
        fill_config(&cfg, 0, 0, 255, i18n_get_lang());
        cfg.cal_valid = true;
    }
    sd_save_config(&cfg);
    use_custom_cal = true;
    Serial.println("[CAL] Saved to SD");
}

void touch_save_settings(uint8_t palette, uint8_t fskip, uint8_t brightness, uint8_t lang) {
    CydGbConfig cfg;
    if (sd_load_config(&cfg)) {
        cfg.palette = palette;
        cfg.frame_skip = fskip;
        cfg.brightness = brightness;
        cfg.language = lang;
        cfg.cal_valid = use_custom_cal;
        if (use_custom_cal) {
            cfg.cal_xmin = cal.x_min;
            cfg.cal_xmax = cal.x_max;
            cfg.cal_ymin = cal.y_min;
            cfg.cal_ymax = cal.y_max;
        }
    } else {
        fill_config(&cfg, palette, fskip, brightness, lang);
    }
    sd_save_config(&cfg);
}

bool touch_load_settings(uint8_t* palette, uint8_t* fskip, uint8_t* brightness, uint8_t* lang) {
    return touch_load_storage(palette, fskip, brightness, lang);
}

bool touch_load_storage(uint8_t* palette, uint8_t* fskip, uint8_t* brightness, uint8_t* lang) {
    CydGbConfig cfg;
    if (!sd_load_config(&cfg)) return false;
    apply_config(&cfg);
    if (palette) *palette = cfg.palette;
    if (fskip) *fskip = cfg.frame_skip;
    if (brightness) *brightness = cfg.brightness;
    if (lang) *lang = cfg.language;
    return true;
}

CydTouchCal touch_get_default_calibration() {
    return {(int16_t)RAW_Y_LO, (int16_t)RAW_Y_HI, (int16_t)RAW_X_LO, (int16_t)RAW_X_HI,
            false, false, false};
}

void touch_init() {
    if (!touch_mtx) touch_mtx = xSemaphoreCreateMutex();
    ts.begin();

    cal = touch_get_default_calibration();
    use_custom_cal = false;
    Serial.println("[CAL] Factory map (load from SD after mount)");

    Serial.printf("[TOUCH] XPT2046_Bitbang MOSI=%d MISO=%d CLK=%d CS=%d (CYD bitbang)\n",
                  TOUCH_PIN_MOSI, TOUCH_PIN_MISO, TOUCH_PIN_CLK, TOUCH_PIN_CS);
}

void touch_reinit() {
    if (touch_mtx) xSemaphoreTake(touch_mtx, portMAX_DELAY);
    reset_gesture();
    ts.begin();
    if (touch_mtx) xSemaphoreGive(touch_mtx);
    Serial.println("[TOUCH] reinit");
}

void touch_set_calibration(CydTouchCal c) {
    cal = c;
    use_custom_cal = true;
}

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

static bool in_rect(int16_t x, int16_t y, int16_t x0, int16_t y0, int16_t x1, int16_t y1) {
    return x >= x0 && x <= x1 && y >= y0 && y <= y1;
}

static bool in_rect_pad(int16_t x, int16_t y, int16_t cx, int16_t cy,
                        int16_t hw, int16_t hh, int16_t pad) {
    return in_rect(x, y, cx - hw - pad, cy - hh - pad, cx + hw + pad, cy + hh + pad);
}

static bool in_square(int16_t x, int16_t y, int16_t cx, int16_t cy, int16_t half, int16_t pad) {
    return in_rect_pad(x, y, cx, cy, half, half, pad);
}

static bool in_circle(int16_t x, int16_t y, int16_t cx, int16_t cy, int16_t r) {
    int dx = x - cx, dy = y - cy;
    return (int32_t)dx * dx + (int32_t)dy * dy <= (int32_t)r * r;
}

static void stick_vector(int16_t x, int16_t y, int16_t* stick_dx, int16_t* stick_dy,
                         uint16_t* btns) {
    int dx = x - STICK_CX, dy = y - STICK_CY;
    int sdx = dx, sdy = dy;
    if (abs(sdx) > STICK_RANGE) sdx = (sdx > 0) ? STICK_RANGE : -STICK_RANGE;
    if (abs(sdy) > STICK_RANGE) sdy = (sdy > 0) ? STICK_RANGE : -STICK_RANGE;
    if (stick_dx) *stick_dx = (int16_t)sdx;
    if (stick_dy) *stick_dy = (int16_t)sdy;

    uint16_t b = 0;
    if (dx * dx + dy * dy >= DPAD_DEAD * DPAD_DEAD) {
        if (dy < -DPAD_AXIS_MIN) b |= GB_BTN_UP;
        if (dy > DPAD_AXIS_MIN) b |= GB_BTN_DOWN;
        if (dx < -DPAD_AXIS_MIN) b |= GB_BTN_LEFT;
        if (dx > DPAD_AXIS_MIN) b |= GB_BTN_RIGHT;
    }
    if (btns) *btns = b;
}

static bool stick_hit_start(int16_t x, int16_t y) {
    int hit_r = STICK_BASE_R + STICK_HIT_EXTRA;
    int dx = x - STICK_CX, dy = y - STICK_CY;
    return dx * dx + dy * dy <= (int32_t)hit_r * hit_r;
}

static uint16_t classify_stick(int16_t x, int16_t y, int16_t* stick_dx, int16_t* stick_dy,
                               bool captured) {
    if (!captured && !stick_hit_start(x, y)) return 0;
    uint16_t b = 0;
    stick_vector(x, y, stick_dx, stick_dy, &b);
    return b;
}

static uint16_t classify_action(int16_t x, int16_t y) {
    if (y < CTRL_Y_MIN) return 0;
    uint16_t b = 0;
    int p = BTN_TOUCH_PAD;
    if (in_rect(x, y, BTN_B_L - p, BTN_AB_T - p,
                BTN_B_L + BTN_B_W + p, BTN_AB_T + BTN_AB_H + p))
        b |= GB_BTN_B;
    if (in_rect(x, y, BTN_A_L - p, BTN_AB_T - p,
                BTN_A_L + BTN_A_W + p, BTN_AB_T + BTN_AB_H + p))
        b |= GB_BTN_A;
    return b;
}

static uint16_t classify_util(int16_t x, int16_t y) {
    if (in_rect(x, y, BTN_PAUSE_L, BTN_PAUSE_T,
                BTN_PAUSE_L + BTN_PAUSE_W, BTN_PAUSE_T + BTN_PAUSE_H))
        return GB_BTN_MENU;

    if (y < CTRL_Y_MIN) return 0;

    if (in_circle(x, y, BTN_ST_X, BTN_ST_Y, BTN_UTIL_HIT))
        return GB_BTN_START;

    if (in_circle(x, y, BTN_SE_X, BTN_SE_Y, BTN_UTIL_HIT))
        return GB_BTN_SELECT;

    return 0;
}

static uint16_t classify(int16_t x, int16_t y) {
    int16_t sdx, sdy;
    return classify_util(x, y) | classify_stick(x, y, &sdx, &sdy, false) | classify_action(x, y);
}

static int16_t median3(int16_t a, int16_t b, int16_t c) {
    if (a > b) { int16_t t = a; a = b; b = t; }
    if (b > c) { int16_t t = b; b = c; c = t; }
    if (a > b) { int16_t t = a; a = b; b = t; }
    return b;
}

static void push_sample(int16_t x, int16_t y, int16_t* fx, int16_t* fy) {
    samp_x[samp_n % SAMPLE_N] = x;
    samp_y[samp_n % SAMPLE_N] = y;
    samp_n++;
    if (samp_n < SAMPLE_N) {
        *fx = x;
        *fy = y;
        return;
    }
    *fx = median3(samp_x[0], samp_x[1], samp_x[2]);
    *fy = median3(samp_y[0], samp_y[1], samp_y[2]);
}

static void reset_gesture() {
    samp_n = 0;
    zone_util = zone_dpad = zone_action = {};
    dpad_vis = 0;
    dpad_stick_dx = dpad_stick_dy = 0;
    dpad_touch = false;
    stick_captured = false;
}

static void zone_feed_instant(TouchZone* z, uint16_t btn, uint32_t now) {
    z->candidate = btn;
    z->latched = btn;
    z->latch_cnt = btn ? ZONE_LATCH : 0;
    if (btn) z->last_ms = now;
}

static void zone_feed(TouchZone* z, uint16_t btn, uint32_t now) {
    if (btn == z->candidate) {
        if (btn && ++z->latch_cnt >= ZONE_LATCH) z->latched = btn;
        else if (!btn) z->latched = 0;
    } else {
        z->candidate = btn;
        z->latch_cnt = btn ? 1 : 0;
        if (!btn) z->latched = 0;
    }
    if (btn) z->last_ms = now;
}

static uint16_t zone_active(const TouchZone* z, uint32_t now) {
    if (now - z->last_ms <= ZONE_RELEASE_MS) return z->latched;
    return 0;
}

static uint16_t merge_zones(uint32_t now) {
    return zone_active(&zone_util, now) |
           zone_active(&zone_dpad, now) |
           zone_active(&zone_action, now);
}

static void update_zones(int16_t x, int16_t y, uint32_t now) {
    uint16_t pause = classify_util(x, y);
    if (pause == GB_BTN_MENU) {
        zone_feed(&zone_util, GB_BTN_MENU, now);
        return;
    }

    if (stick_captured) {
        int16_t sdx = 0, sdy = 0;
        uint16_t btn = classify_stick(x, y, &sdx, &sdy, true);
        zone_feed_instant(&zone_dpad, btn, now);
        dpad_vis = (uint8_t)btn;
        dpad_stick_dx = sdx;
        dpad_stick_dy = sdy;
        dpad_touch = true;
        return;
    }

    if (y < CTRL_Y_MIN) return;

    if (y < ZONE_STICK_Y_MIN) {
        zone_feed(&zone_util, pause, now);
        return;
    }

    if (x <= ZONE_STICK_X_MAX && stick_hit_start(x, y)) {
        stick_captured = true;
        int16_t sdx = 0, sdy = 0;
        uint16_t btn = classify_stick(x, y, &sdx, &sdy, true);
        zone_feed_instant(&zone_dpad, btn, now);
        dpad_vis = (uint8_t)btn;
        dpad_stick_dx = sdx;
        dpad_stick_dy = sdy;
        dpad_touch = true;
    } else if (x >= ZONE_ACTION_X_MIN) {
        zone_feed(&zone_action, classify_action(x, y), now);
    } else if (pause) {
        zone_feed(&zone_util, pause, now);
    }
}

void touch_update() {
    int16_t rx, ry, rz;
    uint32_t now = millis();
    if (read_point(&rx, &ry, &rz)) {
        int16_t mx, my, fx, fy;
        map_screen(rx, ry, &mx, &my);
        push_sample(mx, my, &fx, &fy);
        scr_x = fx;
        scr_y = fy;
        pressed = true;
        last_good_ms = now;

        if (stick_captured || fy >= CTRL_Y_MIN || fy < STATUS_H) {
            update_zones(fx, fy, now);
            cur_btns = merge_zones(now);
            btn_hold = cur_btns;
        } else {
            cur_btns = 0;
            btn_hold = 0;
        }
    } else if (now - last_good_ms < BTN_HOLD_MS) {
        pressed = true;
        cur_btns = merge_zones(now);
        if (!cur_btns) cur_btns = btn_hold;
        if (stick_captured) dpad_touch = true;
    } else {
        pressed = false;
        cur_btns = 0;
        scr_x = scr_y = -1;
        btn_hold = 0;
        dpad_vis = 0;
        dpad_stick_dx = dpad_stick_dy = 0;
        dpad_touch = false;
        reset_gesture();
    }
}

uint16_t touch_get_buttons() { return cur_btns; }
bool touch_is_pressed() { return pressed; }
int16_t touch_get_x() { return scr_x; }
int16_t touch_get_y() { return scr_y; }
uint8_t touch_get_dpad_visual() { return dpad_vis; }
bool touch_dpad_active() { return dpad_touch; }
bool touch_get_dpad_stick(int16_t* dx, int16_t* dy) {
    if (!dpad_touch) return false;
    if (dx) *dx = dpad_stick_dx;
    if (dy) *dy = dpad_stick_dy;
    return true;
}

void touch_format_buttons(uint16_t btn, char* buf, size_t buflen) {
    if (!buf || buflen == 0) return;
    buf[0] = '\0';
    if (!btn) {
        strncpy(buf, "none", buflen - 1);
        buf[buflen - 1] = '\0';
        return;
    }
    struct { uint16_t mask; const char* name; } tbl[] = {
        {GB_BTN_UP, "UP"}, {GB_BTN_DOWN, "DOWN"}, {GB_BTN_LEFT, "LEFT"},
        {GB_BTN_RIGHT, "RIGHT"}, {GB_BTN_A, "A"}, {GB_BTN_B, "B"},
        {GB_BTN_SELECT, "SELECT"}, {GB_BTN_START, "START"}, {GB_BTN_MENU, "MENU"},
    };
    for (size_t i = 0; i < sizeof(tbl) / sizeof(tbl[0]); i++) {
        if (!(btn & tbl[i].mask)) continue;
        if (buf[0]) strncat(buf, "+", buflen - strlen(buf) - 1);
        strncat(buf, tbl[i].name, buflen - strlen(buf) - 1);
    }
}

void touch_run_calibration() {
    tft.fillScreen(TH->bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(TH->accent);
    tft.drawString(tr(STR_CAL_TITLE), SCREEN_CX, 12, 4);
    tft.setTextColor(TH->text_mute);
    tft.drawString(tr(STR_CAL_HINT), SCREEN_CX, 35, 2);

    struct { int16_t sx, sy; } targets[5] = {
        {22, 50}, {SCREEN_W - 22, 50}, {22, SCREEN_H - 50},
        {SCREEN_W - 22, SCREEN_H - 50}, {SCREEN_CX, SCREEN_H / 2}
    };
    static const StringId label_ids[5] = {
        STR_CAL_TL, STR_CAL_TR, STR_CAL_BL, STR_CAL_BR, STR_CAL_CENTER
    };
    int16_t raw_x[5], raw_y[5];
    bool got[5] = {false};

    for (int i = 0; i < 5; i++) {
        tft.fillRect(0, 42, SCREEN_W, 20, TH->bg);
        tft.setTextColor(TH->text_hi);
        char msg[40];
        snprintf(msg, sizeof(msg), tr(STR_CAL_STEP_FMT), i + 1, tr(label_ids[i]));
        tft.drawString(msg, SCREEN_CX, 52, 2);

        int tx = targets[i].sx, ty = targets[i].sy;
        tft.drawCircle(tx, ty, 10, TH->accent);
        tft.drawLine(tx - 14, ty, tx + 14, ty, TH->accent);
        tft.drawLine(tx, ty - 14, tx, ty + 14, TH->accent);

        uint32_t t0 = millis();
        while (millis() - t0 < 15000) {
            int16_t rx, ry, rz;
            if (read_point(&rx, &ry, &rz)) { delay(80); break; }
            delay(10);
        }
        if (millis() - t0 >= 15000) goto cal_fail;

        int16_t samples_x[6], samples_y[6];
        int ns = 0;
        for (int s = 0; s < 6; s++) {
            int16_t rx, ry, rz;
            if (read_point(&rx, &ry, &rz)) {
                samples_x[ns] = rx;
                samples_y[ns] = ry;
                ns++;
            }
            delay(20);
        }

        if (ns >= 2) {
            for (int a = 0; a < ns - 1; a++)
                for (int b = a + 1; b < ns; b++) {
                    if (samples_x[a] > samples_x[b]) {
                        int16_t t = samples_x[a]; samples_x[a] = samples_x[b]; samples_x[b] = t;
                    }
                    if (samples_y[a] > samples_y[b]) {
                        int16_t t = samples_y[a]; samples_y[a] = samples_y[b]; samples_y[b] = t;
                    }
                }
            raw_x[i] = samples_x[ns / 2];
            raw_y[i] = samples_y[ns / 2];
            got[i] = true;
            Serial.printf("[CAL] %d: raw(%d,%d) screen(%d,%d)\n", i, raw_x[i], raw_y[i], tx, ty);
        }

        tft.fillCircle(tx, ty, 8, got[i] ? TH->menu_primary : TH->menu_danger);

        t0 = millis();
        while (millis() - t0 < 3000) {
            int16_t rx, ry, rz;
            if (!read_point(&rx, &ry, &rz)) break;
            delay(10);
        }
        delay(200);
    }

    {
        int valid = 0;
        int16_t ry_min = 32767, ry_max = -32768;
        int16_t rx_min = 32767, rx_max = -32768;
        for (int i = 0; i < 5; i++) {
            if (!got[i]) continue;
            valid++;
            if (raw_y[i] < ry_min) ry_min = raw_y[i];
            if (raw_y[i] > ry_max) ry_max = raw_y[i];
            if (raw_x[i] < rx_min) rx_min = raw_x[i];
            if (raw_x[i] > rx_max) rx_max = raw_x[i];
        }
        if (valid < 5) goto cal_fail;

        CydTouchCal nc = {0};
        int16_t ry_span = ry_max - ry_min;
        int16_t rx_span = rx_max - rx_min;
        if (ry_span < 200 || rx_span < 200) goto cal_fail;

        nc.x_min = ry_min - ry_span * 12 / 80;
        nc.x_max = ry_max + ry_span * 12 / 80;
        nc.y_min = rx_min - rx_span * 15 / 75;
        nc.y_max = rx_max + rx_span * 15 / 75;

        cal = nc;
        save_cal_to_sd();
        Serial.printf("[CAL] OK rawY[%d-%d] rawX[%d-%d]\n",
                      cal.x_min, cal.x_max, cal.y_min, cal.y_max);
    }

    tft.fillScreen(TH->bg);
    tft.setTextColor(TH->menu_primary);
    tft.drawString(tr(STR_CAL_SAVED), SCREEN_CX, SCREEN_H / 2, 4);
    delay(1200);
    return;

cal_fail:
    tft.fillScreen(TH->bg);
    tft.setTextColor(TH->menu_danger);
    tft.drawString(tr(STR_CAL_FAILED), SCREEN_CX, SCREEN_H / 2 - 20, 4);
    tft.setTextColor(TH->text_mute);
    tft.drawString(tr(STR_CAL_FACTORY), SCREEN_CX, SCREEN_H / 2 + 20, 2);
    use_custom_cal = false;
    delay(2000);
}
