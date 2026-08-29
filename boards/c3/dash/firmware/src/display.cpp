#include "display.h"

#include <Adafruit_ST7735.h>
#include <SPI.h>
#include <lvgl.h>

#include "pins.h"

static Adafruit_ST7735 tft(&SPI, TFT_CS, TFT_DC, TFT_RST);
static lv_display_t *sDisplay = nullptr;

static const int kLines = 32;
static uint8_t sBuf[UI_W * kLines * 2];

static uint32_t tickCb() { return millis(); }

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const int w = area->x2 - area->x1 + 1;
  const int h = area->y2 - area->y1 + 1;

  lv_draw_sw_rgb565_swap(px_map, w * h);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.writePixels(reinterpret_cast<uint16_t *>(px_map), w * h, true, true);
  tft.endWrite();

  lv_display_flush_ready(disp);
}

void displayBegin() {
  SPI.begin(TFT_SCLK, -1, TFT_MOSI, TFT_CS);
  tft.initR(INITR_144GREENTAB);
  tft.setSPISpeed(20000000);
  tft.setRotation(2);
  tft.fillScreen(0x0000);

  lv_init();
  lv_tick_set_cb(tickCb);

  sDisplay = lv_display_create(UI_W, UI_H);
  lv_display_set_flush_cb(sDisplay, flushCb);
  lv_display_set_buffers(sDisplay, sBuf, nullptr, sizeof(sBuf),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void displayTask() { lv_timer_handler(); }
