#include <Arduino.h>
#include "hw_config.h"
#include "display.h"
#include "touch_input.h"
#include "sd_manager.h"
#include "ui_launcher.h"
#include "emulator_bridge.h"

static RomEntry roms[64];
static int rcnt = 0;
static char cur_path[80] = {0};
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
static void tt_stop() { if(ttask) vTaskSuspend(ttask); }

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

// ─── Emulation loop ─────────────────────────────────────────────────────────
void run_emu() {
    emu_on = true; menu_req = false; menu_btn_prev = false;
    display_clear(TFT_BLACK);
    display_draw_controls();

    uint32_t ft=0;
    while(emu_on) {
        emu_run_frame();

        if (menu_req) {
            menu_req = false;

            int c = launcher_ingame_menu();
            switch(c) {
                case 0: break;  // resume
                case 1:  // save
                    save_ram();
                    tft.fillRect(SCREEN_CX-80,80,160,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM); tft.setTextColor(TFT_GREEN);
                    tft.drawString("SAVED!",SCREEN_CX,100,4);
                    delay(700);
                    break;
                case 2:  // load
                    load_ram(); emu_reset(); load_ram();
                    tft.fillRect(SCREEN_CX-80,80,160,40,TFT_BLACK);
                    tft.setTextDatum(MC_DATUM); tft.setTextColor(0x07FF);
                    tft.drawString("LOADED!",SCREEN_CX,100,4);
                    delay(700);
                    break;
                case 3:  // quit
                    emu_on=false; save_ram(); return;
                case 4:  // calibrate
                    touch_run_calibration(); break;
                case 5:  // settings
                    launcher_settings_menu(); break;
            }
            display_clear(TFT_BLACK);
            display_draw_controls();
        }

        // FPS log
        uint32_t n=millis();
        if(n-ft>3000){
            ft=n;
            char lbl[48];
            touch_format_buttons(touch_get_buttons(), lbl, sizeof(lbl));
            Serial.printf("[G] FPS:%u btn:%s\n", emu_get_fps(), lbl);
        }
        taskYIELD();
    }
}

// ─── Setup ──────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200); delay(200);
    Serial.println("\n=== CYD-GB ===");
    pinMode(LED_R_PIN,OUTPUT); pinMode(LED_G_PIN,OUTPUT); pinMode(LED_B_PIN,OUTPUT);
    digitalWrite(LED_R_PIN,HIGH); digitalWrite(LED_G_PIN,HIGH); digitalWrite(LED_B_PIN,HIGH);

    display_init();
    touch_init();
    tt_start();

    if(!sd_init()) {
        tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED); tft.drawString("SD Card Error!",SCREEN_CX,120,4);
        tft.setTextColor(0x7BEF); tft.drawString("Insert FAT32 SD & reset",SCREEN_CX,160,2);
        while(true) delay(1000);
    }

    // Splash
    tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x07E0); tft.drawString("CYD-GB",SCREEN_CX,120,4);
    tft.setTextColor(0x7BEF); tft.drawString("Game Boy Emulator",SCREEN_CX,160,2);
    delay(1200);

    uint8_t s_pal, s_fs, s_bl;
    if (touch_load_storage(&s_pal, &s_fs, &s_bl)) {
        emu_set_palette(s_pal);
        emu_set_frame_skip(s_fs);
        display_set_backlight(s_bl);
        Serial.printf("[INIT] Settings from SD: pal=%d fs=%d bl=%d\n", s_pal, s_fs, s_bl);
    } else {
        Serial.println("[INIT] No config on SD — defaults");
    }

    Serial.printf("[INIT] Heap: %u\n",ESP.getFreeHeap());
}

// ─── Loop ───────────────────────────────────────────────────────────────────
void loop() {
    rcnt = sd_scan_roms(roms, 64);
    int sel = launcher_show(roms, rcnt);
    if(sel==-2){touch_run_calibration();return;}
    if(sel==-3){launcher_settings_menu();return;}
    if(sel<0||sel>=rcnt) return;

    strncpy(cur_path,roms[sel].full_path,79);

    // Loading screen
    tft.fillScreen(TFT_BLACK); tft.setTextDatum(MC_DATUM);
    tft.setTextColor(0x07E0); tft.drawString("Loading...",SCREEN_CX,120,4);
    char nm[30]; strncpy(nm,roms[sel].filename,28); nm[28]=0;
    char* d=strrchr(nm,'.'); if(d)*d=0;
    tft.setTextColor(TFT_WHITE); tft.drawString(nm,SCREEN_CX,160,2);

    if(!emu_open_rom(cur_path)){
        tft.setTextColor(TFT_RED); tft.drawString("Open failed!",SCREEN_CX,200,2); delay(2000); return;
    }
    if(!emu_init(0,0)){
        tft.setTextColor(TFT_RED); tft.drawString("Init failed!",SCREEN_CX,200,2); delay(2000); emu_close_rom(); return;
    }

    load_ram();
    emu_set_frame_skip(0);
    digitalWrite(LED_G_PIN,LOW);
    run_emu();
    digitalWrite(LED_G_PIN,HIGH);
    emu_close_rom();
}
