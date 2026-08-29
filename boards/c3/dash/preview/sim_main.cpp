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
  screensAir(34, "boa", 8.4f, 14.0f, 9.0f);
  screensMoon("gibosa crescente", 0.72f, true);
  screensWind(18.0f, 135, 34.0f, 1013.0f, 1.4f);

  const float rain[8] = {0, 0, 0.2f, 1.4f, 2.8f, 1.1f, 0.3f, 0};
  screensRain(rain, 8, 30);
  screensSun2("06:21", "18:04", "11h 43min de sol", 62);
  screensMarket(5.42f, 0.37f, 5.91f, -0.22f, 348000.0f, 2.4f);
  screensDev(7, 12, 2, 5, 34, "015-esp-projects");
  screensTimer("18:24", "foco", 26, true);
  screensNews("Daniel Cargnin e ouro no Grand Slam de Lausanne de judo",
              "Mega-Sena sorteia premio acumulado de R$ 30 milhoes neste domingo",
              "Ministerio da Saude reforca chamamento para vacina contra o sarampo", 3);
  screensRates(14.00f, 13.90f, 4.44f);
  screensHoliday("Independencia do Brasil", "2026-09-07", 9);
  screensHistory(1949,
                 "E realizado o primeiro teste de uma bomba nuclear da Uniao Sovietica, a RDS-1, "
                 "em Semipalatinsk.");
  screensSpace(12, -35.9f, -29.1f);
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
  screensAir(142, "pessima", 188.6f, 240.0f, 12.0f);
  screensMoon("quarto minguante", 0.5f, false);
  screensWind(128.0f, 315, 180.0f, 998.0f, -3.2f);

  const float storm[8] = {8.4f, 12.2f, 15.0f, 9.9f, 4.2f, 2.0f, 0.5f, 0};
  screensRain(storm, 8, 0);
  screensSun2("05:59", "19:12", "13h 13min de sol", 100);
  screensMarket(19.99f, -12.5f, 21.44f, -8.7f, 1200000.0f, 15.2f);
  screensDev(0, 0, 0, 0, 0, "monorepo-com-nome-gigante");
  screensTimer("00:00", "pausa", 100, false);
  screensNews("Governo anuncia pacote de medidas economicas para conter a alta dos combustiveis e "
              "a inflacao de alimentos no segundo semestre",
              "--", "--", 1);
  screensRates(999.99f, 0.0f, -1.25f);
  screensHoliday("Nossa Senhora Aparecida - Padroeira do Brasil", "2026-10-12", 365);
  screensHistory(1500,
                 "Uma sequencia muito longa de acontecimentos historicos ocorre simultaneamente em "
                 "diversos continentes distintos e precisa ser truncada pela interface.");
  screensSpace(0, 0.0f, 0.0f);
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
