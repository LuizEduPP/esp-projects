#include <Arduino.h>
#include "hw_config.h"
#include "display.h"
#include "touch_input.h"
#include "game_catalog.h"
#include "ui_calib.h"
#include "ui_launcher.h"
#include "ui_settings.h"
#include "game_runtime.h"
#include "buzzer.h"

static GameEntry games[MAX_GAMES];

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n=== CYD-ARCADE (TFT) ===");

    pinMode(LED_R_PIN, OUTPUT);
    pinMode(LED_G_PIN, OUTPUT);
    pinMode(LED_B_PIN, OUTPUT);
    digitalWrite(LED_R_PIN, HIGH);
    digitalWrite(LED_G_PIN, HIGH);
    digitalWrite(LED_B_PIN, HIGH);

    display_init();
    touch_init();
    buzzer_init();

    display_splash("CYD-ARCADE");
    buzzer_play(SFX_STARTUP);
    if (display_splash_wait(SPLASH_MS)) {
        touch_clear_calibration();
        Serial.println("[INIT] calibracao resetada (segurou na splash)");
    }

    if (!touch_has_saved_calibration()) {
        Serial.println("[INIT] sem calibracao — abrindo assistente");
        ui_calib_run();
    }

    Serial.printf("[INIT] heap=%u games=%d cal=%d\n",
                  ESP.getFreeHeap(), game_catalog_count(), touch_has_saved_calibration());
}

void loop() {
    if (!touch_has_saved_calibration()) {
        ui_calib_run();
        return;
    }

    const int n = game_catalog_count();
    game_catalog_list(games, MAX_GAMES);

    const int sel = launcher_show(games, n);
    if (sel == LAUNCHER_SETTINGS) {
        if (ui_settings_show() == UI_SETTINGS_CALIBRATE) {
            touch_clear_calibration();
            ui_calib_run();
        }
        return;
    }
    if (sel < 0 || sel >= n) return;

    digitalWrite(LED_G_PIN, LOW);
    game_run(&games[sel]);
    digitalWrite(LED_G_PIN, HIGH);
}
