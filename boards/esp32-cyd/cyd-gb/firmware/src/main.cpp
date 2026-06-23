#include <Arduino.h>
#include "hw_config.h"
#include "display.h"
#include "touch_input.h"
#include "sd_manager.h"
#include "ui_launcher.h"
#include "ui_screens.h"
#include "emulator_bridge.h"
#include "i18n.h"
#include "ui_theme.h"

static RomEntry roms[64];
static int rcnt = 0;
static char cur_path[80] = {0};
static char cur_title[32] = {0};
static TaskHandle_t ttask = nullptr;
static volatile bool emu_on = false, menu_req = false;
static volatile bool menu_btn_prev = false;

void touch_task(void* p) {
    (void)p;
    static uint16_t prev_btn = 0;
    for(;;) {
        touch_update();
        if (emu_on) {
            uint16_t b = touch_get_buttons();
            if (b != prev_btn) {
                char lbl[48];
                touch_format_buttons(b, lbl, sizeof(lbl));
                Serial.printf("[G] touch %d,%d -> %s (0x%03X)\n",
                              touch_get_x(), touch_get_y(), lbl, b);
                prev_btn = b;
            }
            bool menu_now = (b & GB_BTN_MENU) != 0;
            if (menu_now && !menu_btn_prev) menu_req = true;
            menu_btn_prev = menu_now;
            emu_set_joypad(b & 0xFF);
        } else {
            prev_btn = 0;
        }
        vTaskDelay(1);
    }
}

static void tt_start() {
    if(!ttask) xTaskCreatePinnedToCore(touch_task,"t",4096,0,5,&ttask,0);
    else vTaskResume(ttask);
}

static void save_ram() {
    if(!cur_path[0]) return;
    uint32_t sz=0; uint8_t* r=emu_get_cart_ram(&sz);
    if(sz>0) { sd_save_state(cur_path,r,sz); Serial.printf("[SAVE] %u bytes\n",sz); }
}
static void load_ram() {
    if(!cur_path[0]) return;
    uint32_t sz=0; emu_get_cart_ram(&sz);
    if(sz>0) { uint8_t* t=(uint8_t*)malloc(sz);
        if(t){if(sd_load_state(cur_path,t,sz))emu_set_cart_ram(t,sz);free(t);} }
}

static void redraw_game_ui() {
    emu_set_palette(emu_get_palette());
    display_draw_game_frame();
    display_draw_status_bar(cur_title, emu_get_fps());
    display_draw_controls();
}

void run_emu() {
    emu_on = true; menu_req = false; menu_btn_prev = false;
    display_clear(TFT_BLACK);
    redraw_game_ui();

    uint32_t ft = 0;
    uint32_t fps_t = 0;
    uint8_t last_dpad = 0;
    int16_t last_sdx = 0, last_sdy = 0;

    while(emu_on) {
        emu_run_frame();

        if (touch_dpad_active()) {
            uint8_t dpad = touch_get_dpad_visual();
            int16_t sdx = 0, sdy = 0;
            touch_get_dpad_stick(&sdx, &sdy);
            if (dpad != last_dpad || abs(sdx - last_sdx) > 2 || abs(sdy - last_sdy) > 2) {
                display_update_dpad(dpad, sdx, sdy);
                last_dpad = dpad;
                last_sdx = sdx;
                last_sdy = sdy;
            }
        } else if (last_dpad || last_sdx || last_sdy) {
            display_reset_dpad();
            last_dpad = 0;
            last_sdx = last_sdy = 0;
        }

        uint32_t now = millis();
        if (now - fps_t > 1000) {
            fps_t = now;
            display_update_status_fps(emu_get_fps());
        }

        if (menu_req) {
            menu_req = false;

            int c = launcher_ingame_menu();
            switch(c) {
                case 0: break;
                case 1:
                    save_ram();
                    ui_show_toast(tr(STR_SAVED), ui_theme_get()->ok);
                    break;
                case 2:
                    load_ram(); emu_reset(); load_ram();
                    ui_show_toast(tr(STR_LOADED), ui_theme_get()->accent);
                    break;
                case 3:
                    emu_on=false; save_ram(); return;
                case 4:
                    touch_run_calibration(); break;
                case 5:
                    launcher_settings_menu(); break;
            }
            redraw_game_ui();
            last_dpad = 0;
            last_sdx = last_sdy = 0;
        }

        if(now-ft>3000){
            ft=now;
            char lbl[48];
            touch_format_buttons(touch_get_buttons(), lbl, sizeof(lbl));
            Serial.printf("[G] FPS:%u btn:%s\n", emu_get_fps(), lbl);
        }
        taskYIELD();
    }
}


void setup() {
    Serial.begin(115200); delay(200);
    Serial.println("\n=== CYD-GB ===");
    i18n_set_lang(LANG_EN);
    pinMode(LED_R_PIN,OUTPUT); pinMode(LED_G_PIN,OUTPUT); pinMode(LED_B_PIN,OUTPUT);
    digitalWrite(LED_R_PIN,HIGH); digitalWrite(LED_G_PIN,HIGH); digitalWrite(LED_B_PIN,HIGH);

    display_init();
    touch_init();
    tt_start();

    if (!sd_init()) {
        ui_draw_sd_error();
        while(true) delay(1000);
    }

    uint8_t s_pal, s_fs, s_bl, s_lang;
    if (touch_load_storage(&s_pal, &s_fs, &s_bl, &s_lang)) {
        Serial.printf("[INIT] Settings from SD: pal=%d fs=%d bl=%d lang=%u\n", s_pal, s_fs, s_bl, s_lang);
    } else {
        i18n_set_lang(LANG_EN);
        emu_set_palette(0);
        Serial.println("[INIT] No config on SD — defaults");
    }

    ui_animate_splash(1200);

    Serial.printf("[INIT] Heap: %u\n",ESP.getFreeHeap());
}


void loop() {
    rcnt = sd_scan_roms(roms, 64);
    int sel = launcher_show(roms, rcnt);
    if(sel==-2){touch_run_calibration();return;}
    if(sel==-3){launcher_settings_menu();return;}
    if(sel<0||sel>=rcnt) return;

    strncpy(cur_path,roms[sel].full_path,79);
    strncpy(cur_title, roms[sel].filename, 31);
    char* d = strrchr(cur_title, '.');
    if (d) *d = 0;

    auto load_rom = []() -> bool {
        return emu_open_rom(cur_path);
    };

    if (!ui_loading_run(cur_title, load_rom)) {
        tft.fillScreen(ui_theme_get()->bg);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(ui_theme_get()->danger);
        tft.drawString(tr(STR_OPEN_FAILED), SCREEN_CX, 160, 2);
        delay(2000);
        return;
    }
    if(!emu_init(0,0)){
        tft.fillScreen(ui_theme_get()->bg);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(ui_theme_get()->danger);
        tft.drawString(tr(STR_INIT_FAILED), SCREEN_CX, 160, 2);
        delay(2000);
        emu_close_rom();
        return;
    }

    load_ram();
    digitalWrite(LED_G_PIN,LOW);
    run_emu();
    digitalWrite(LED_G_PIN,HIGH);
    emu_close_rom();
}
