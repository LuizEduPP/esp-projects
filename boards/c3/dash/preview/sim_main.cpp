#include <stdio.h>
#include <string.h>

#include <lvgl.h>

#include "screens.h"

#define W 128
#define H 128

static uint16_t framebuffer[W * H];
static uint32_t fakeTick;

static uint32_t tickCb(void) { return fakeTick; }

static void flushCb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  const uint16_t *src = (const uint16_t *)px_map;
  for (int y = area->y1; y <= area->y2; ++y) {
    for (int x = area->x1; x <= area->x2; ++x) {
      framebuffer[y * W + x] = *src++;
    }
  }
  lv_display_flush_ready(disp);
}

static void savePPM(const char *path) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  for (int i = 0; i < W * H; ++i) {
    const uint16_t c = framebuffer[i];
    const unsigned char rgb[3] = {
        (unsigned char)(((c >> 11) & 0x1F) * 255 / 31),
        (unsigned char)(((c >> 5) & 0x3F) * 255 / 63),
        (unsigned char)((c & 0x1F) * 255 / 31),
    };
    fwrite(rgb, 1, 3, f);
  }
  fclose(f);
}

static void settle(uint32_t ms) {
  for (uint32_t i = 0; i < ms; i += 10) {
    fakeTick += 10;
    lv_timer_handler();
  }
}

int main(int argc, char **argv) {
  const char *outDir = argc > 1 ? argv[1] : ".";

  lv_init();
  lv_tick_set_cb(tickCb);

  static uint8_t buf[W * 40 * 2];
  lv_display_t *disp = lv_display_create(W, H);
  lv_display_set_flush_cb(disp, flushCb);
  lv_display_set_buffers(disp, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

  screensBuild();

  screensCity("CESARIO LANGE");
  screensClock("15:47", 38, "SABADO", "29 de agosto");
  screensWeather(27.9f, "parc. nublado", 62, 20.0f, 29.0f, 2);
  screensForecast(0, "DOM", 19.0f, 30.0f, 0);
  screensForecast(1, "SEG", 21.0f, 27.0f, 61);
  screensForecast(2, "TER", 18.0f, 24.0f, 3);

  float hours[24];
  for (int i = 0; i < 24; ++i) {
    const float t = 20.0f + 7.0f * (float)(i % 12) / 11.0f - (i > 14 ? 3.0f : 0.0f);
    hours[i] = t;
  }
  screensChart(hours, 24);
  screensSun("06:21", "18:04");
  screensInsight("O ceu encoberto promete uma tarde quente e umida.", false, "15:40");
  screensSystem("hotspot", -58, "10.31.254.103", 187, "0h 12m");

  char path[512];
  for (int p = 0; p < UI_PAGES; ++p) {
    screensGoTo(p, false);
    settle(600);
    snprintf(path, sizeof(path), "%s/page%d.ppm", outDir, p);
    savePPM(path);
  }

  screensCity("SAO JOSE DOS CAMPOS");
  screensClock("00:00", 59, "QUARTA-FEIRA", "30 de setembro");
  screensWeather(-12.5f, "trovoada com granizo forte", 100, -12.0f, 100.0f, 95);
  screensForecast(0, "QUARTA", -12.0f, 100.0f, 95);
  screensForecast(1, "QUINTA", -8.0f, 41.0f, 61);
  screensForecast(2, "SEXTA", 100.0f, 100.0f, 3);
  screensSun("06:21", "18:04");
  screensInsight(
      "Uma frente fria bastante intensa chega durante a madrugada e derruba a temperatura de "
      "forma abrupta em toda a regiao, entao leve agasalho.",
      false, "23:59");
  screensSystem("MinhaRedeWiFi5GHz", -100, "192.168.100.254", 1024, "999h 59m");

  for (int p = 0; p < UI_PAGES; ++p) {
    screensGoTo(p, false);
    settle(600);
    snprintf(path, sizeof(path), "%s/stress%d.ppm", outDir, p);
    savePPM(path);
  }

  screensShowProvisioning(true);
  settle(600);
  snprintf(path, sizeof(path), "%s/prov.ppm", outDir);
  savePPM(path);

  printf("ok\n");
  return 0;
}
