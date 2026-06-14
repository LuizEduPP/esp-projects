#include "ui_calib.h"
#include "touch_input.h"
#include "display.h"
#include "hw_config.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>

#define TH ui_theme_get()

static const int16_t k_tx[5] = {36, 204, 36, 204, 120};
static const int16_t k_ty[5] = {148, 148, 280, 280, 214};

static bool ls_fit(const int16_t* raw, const int16_t* scr, int n, float* a, float* b) {
    int64_t sr = 0, ss = 0, srr = 0, srs = 0;
    for (int i = 0; i < n; i++) {
        sr += raw[i];
        ss += scr[i];
        srr += (int64_t)raw[i] * raw[i];
        srs += (int64_t)raw[i] * scr[i];
    }
    const float fn = (float)n;
    const float den = fn * (float)srr - (float)sr * (float)sr;
    if (fabsf(den) < 1.0f) return false;
    *a = (fn * (float)srs - (float)sr * (float)ss) / den;
    *b = ((float)ss - (*a) * (float)sr) / fn;
    return true;
}

static void draw_target(int step) {
    const int cx = k_tx[step];
    const int cy = k_ty[step];
    const int r = 28;
    tft.fillCircle(cx, cy, r + 4, TH->card);
    tft.drawCircle(cx, cy, r + 4, TH->border);
    tft.drawCircle(cx, cy, r, TH->accent_hi);
    tft.drawCircle(cx, cy, r - 6, TH->accent);
    tft.fillCircle(cx, cy, 4, TH->ok);
    tft.drawLine(cx - r + 8, cy, cx + r - 8, cy, TH->border);
    tft.drawLine(cx, cy - r + 8, cx, cy + r - 8, TH->border);
}

static void draw_calib_screen(int step) {
    tft.fillScreen(TH->bg);
    char sub[24];
    snprintf(sub, sizeof(sub), "Alvo %d de 5", step + 1);
    ui_draw_app_header(UI_ICON_TARGET, "Calibrar toque", sub);

    tft.setTextDatum(TC_DATUM);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString("Toque no centro de cada alvo", SCREEN_CX, 68, 2);

    for (int i = 0; i < 5; i++) {
        const int dx = SCREEN_CX - 40 + i * 20;
        tft.fillCircle(dx, 92, i <= step ? 4 : 3, i < step ? TH->accent_hi :
                       (i == step ? TH->accent : TH->border));
    }

    draw_target(step);

    tft.setTextDatum(BC_DATUM);
    tft.setTextColor(TH->text_mute, TH->bg);
    tft.drawString("toque no centro do alvo", SCREEN_CX, SCREEN_H - 12, 1);
}

void ui_calib_run() {
    int16_t raw_x[5], raw_y[5];
    int step = 0;
    uint32_t last_hit = 0;

    draw_calib_screen(step);
    touch_wait_release();
    Serial.println("[CALIB] aguardando 5 toques");

    while (step < 5) {
        touch_poll();
        if (touch_is_pressed()) {
            int16_t rx, ry;
            if (touch_read_raw(&rx, &ry) && millis() - last_hit > 400) {
                raw_x[step] = rx;
                raw_y[step] = ry;
                step++;
                last_hit = millis();
                Serial.printf("[CALIB] ponto %d raw=%d,%d\n", step, rx, ry);
                touch_wait_release();
                if (step < 5) draw_calib_screen(step);
            }
        }
        delay(5);
    }

    bool ok = false;
    int16_t scr_xp[5], scr_yp[5];
    for (int i = 0; i < 5; i++) {
        scr_xp[i] = k_tx[i];
        scr_yp[i] = k_ty[i];
    }

    float ax, bx, ay, by;
    if (ls_fit(raw_y, scr_xp, 5, &ax, &bx) && ls_fit(raw_x, scr_yp, 5, &ay, &by)) {
        CydTouchCal nc;
        const float span_x = -239.0f / ax;
        const float span_y = 319.0f / ay;
        nc.x_max = (int16_t)(-bx / ax);
        nc.x_min = (int16_t)(nc.x_max - span_x);
        nc.y_min = (int16_t)(-by / ay);
        nc.y_max = (int16_t)(nc.y_min + span_y);
        touch_set_calibration(nc);
        ok = true;
        Serial.printf("[CALIB] ok xmin=%d xmax=%d ymin=%d ymax=%d\n",
                      nc.x_min, nc.x_max, nc.y_min, nc.y_max);
    } else {
        Serial.println("[CALIB] regressao falhou");
    }

    tft.fillScreen(TH->bg);
    const int pw = 160;
    const int ph = 72;
    const int px = (SCREEN_W - pw) / 2;
    const int py = SCREEN_H / 2 - ph / 2;
    tft.fillRoundRect(px, py, pw, ph, 8, TH->bg);
    tft.drawRoundRect(px, py, pw, ph, 8, ok ? TH->ok : TH->accent);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(ok ? TH->ok : TH->accent, TH->bg);
    tft.drawString(ok ? "Calibrado!" : "Falhou", SCREEN_CX, SCREEN_H / 2, 4);
    delay(1000);
}
