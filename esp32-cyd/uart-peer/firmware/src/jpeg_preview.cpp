#include "jpeg_preview.h"

#include "firmware_version.h"
#include "ui_layout.h"

static bool gContentCleared = false;

void jpegPreviewBegin(LGFX_CYD &tft) {
  tft.setSwapBytes(true);
  gContentCleared = false;
}

bool jpegPreviewShow(LGFX_CYD &tft, const uint8_t *data, size_t len) {
  const int x0 = 0;
  const int y0 = uiContentY(tft);
  const int w = tft.width();
  const int h = uiContentH(tft);

  tft.startWrite();
  // Só limpa na 1ª frame — evita flash preto entre updates do stream.
  if (!gContentCleared) {
    tft.fillRect(x0, y0, w, h, TFT_BLACK);
    gContentCleared = true;
  }
  const bool ok = tft.drawJpg(data, static_cast<uint32_t>(len), x0, y0);
  tft.endWrite();
  return ok;
}

void jpegPreviewDrawStatus(LGFX_CYD &tft, const char *line1, const char *line2) {
  tft.fillRect(0, 0, tft.width(), UI_HEADER_H, TFT_NAVY);
  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(1);
  tft.setCursor(4, 3);
  tft.print(line1);
  if (line2) {
    tft.setCursor(4, 10);
    tft.print(line2);
  }

  char verLabel[16];
  snprintf(verLabel, sizeof(verLabel), "v%s", FW_VERSION);
  tft.setTextColor(TFT_CYAN);
  tft.setCursor(tft.width() - tft.textWidth(verLabel) - 4, 3);
  tft.print(verLabel);

  tft.fillRect(0, uiFooterY(tft), tft.width(), UI_FOOTER_H, TFT_BLACK);
}
