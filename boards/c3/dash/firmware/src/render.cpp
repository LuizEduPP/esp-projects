#include "render.h"

#include <math.h>
#include <string.h>

#include "data.h"

#define W 128
#define H 128
#define MAX_BINDS 28

enum BindKind { BIND_LABEL, BIND_ARC, BIND_BAR, BIND_CHART, BIND_ICON, BIND_NEEDLE };

struct Bind {
  lv_obj_t *obj;
  BindKind kind;
  char path[36];
  char fmt[20];
  float min;
  float max;
};

static Bind sBinds[MAX_BINDS];
static int sBindCount = 0;
static lv_chart_series_t *sSeries[2];
static int sSeriesCount = 0;

static const lv_font_t *fontOf(int size) {
  switch (size) {
    case 48:
      return &lv_font_montserrat_48;
    case 40:
      return &lv_font_montserrat_40;
    case 34:
      return &lv_font_montserrat_34;
    case 28:
      return &lv_font_montserrat_28;
    case 24:
      return &lv_font_montserrat_24;
    case 20:
      return &lv_font_montserrat_20;
    case 16:
      return &lv_font_montserrat_16;
    case 14:
      return &lv_font_montserrat_14;
    case 10:
      return &lv_font_montserrat_10;
    default:
      return &lv_font_montserrat_12;
  }
}

static uint32_t colorOf(const char *hex, uint32_t fallback = 0xFFFFFF) {
  if (!hex || hex[0] != '#') return fallback;
  return (uint32_t)strtoul(hex + 1, nullptr, 16);
}

static void addBind(lv_obj_t *obj, BindKind kind, const char *path, const char *fmt, float min,
                    float max) {
  if (!path || !path[0] || sBindCount >= MAX_BINDS) return;
  Bind &b = sBinds[sBindCount++];
  b.obj = obj;
  b.kind = kind;
  snprintf(b.path, sizeof(b.path), "%s", path);
  snprintf(b.fmt, sizeof(b.fmt), "%s", fmt ? fmt : "");
  b.min = min;
  b.max = max;
}

static void driftY(void *obj, int32_t v) { lv_obj_set_y((lv_obj_t *)obj, v); }

static void scrollWrapped(lv_obj_t *label) {
  lv_obj_update_layout(label);
  lv_anim_delete(label, driftY);
  lv_obj_set_y(label, 0);

  const int boxH = lv_obj_get_height(lv_obj_get_parent(label));
  const int textH = lv_obj_get_height(label);
  if (textH <= boxH) return;

  const int travel = textH - boxH;
  lv_anim_t a;
  lv_anim_init(&a);
  lv_anim_set_var(&a, label);
  lv_anim_set_exec_cb(&a, driftY);
  lv_anim_set_values(&a, 0, -travel);
  lv_anim_set_duration(&a, travel * 140);
  lv_anim_set_playback_duration(&a, travel * 140);
  lv_anim_set_delay(&a, 2000);
  lv_anim_set_playback_delay(&a, 2000);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
}

static lv_obj_t *bare(lv_obj_t *par, int x, int y, int w, int h) {
  lv_obj_t *o = lv_obj_create(par);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  return o;
}

static void buildBackground(lv_obj_t *par, JsonObjectConst bg) {
  const char *kind = bg["kind"] | "solid";
  const uint32_t from = colorOf(bg["from"] | "#000000", 0x000000);

  lv_obj_set_style_bg_color(par, lv_color_hex(from), 0);
  lv_obj_set_style_bg_opa(par, LV_OPA_COVER, 0);

  if (strcmp(kind, "gradient") == 0) {
    lv_obj_set_style_bg_grad_color(par, lv_color_hex(colorOf(bg["to"] | "#000000", from)), 0);
    lv_obj_set_style_bg_grad_dir(par, LV_GRAD_DIR_VER, 0);
  } else {
    lv_obj_set_style_bg_grad_dir(par, LV_GRAD_DIR_NONE, 0);
  }

  if (strcmp(kind, "grid") == 0 || strcmp(kind, "dots") == 0) {
    const int step = bg["step"] | 8;
    const uint32_t line = colorOf(bg["line"] | "#222222", 0x222222);
    const bool dots = strcmp(kind, "dots") == 0;

    for (int y = step; y < H; y += step) {
      for (int x = dots ? step : 0; x < W; x += dots ? step : W) {
        lv_obj_t *o = bare(par, x, y, dots ? 1 : W, 1);
        lv_obj_set_style_bg_color(o, lv_color_hex(line), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
      }
    }
    if (!dots) {
      for (int x = step; x < W; x += step) {
        lv_obj_t *o = bare(par, x, 0, 1, H);
        lv_obj_set_style_bg_color(o, lv_color_hex(line), 0);
        lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
      }
    }
  }

  for (JsonObjectConst b : bg["blobs"].as<JsonArrayConst>()) {
    const int cx = b["x"] | 0;
    const int cy = b["y"] | 0;
    const int r = b["r"] | 20;
    const uint32_t color = colorOf(b["color"] | "#ffffff");
    const int opa = b["opa"] | 60;

    for (int i = 3; i >= 1; --i) {
      const int rr = r * i / 3;
      lv_obj_t *o = bare(par, cx - rr, cy - rr, rr * 2, rr * 2);
      lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
      lv_obj_set_style_bg_opa(o, opa / 3, 0);
    }
  }
}

static void buildLabel(lv_obj_t *par, JsonObjectConst it) {
  const int x = it["x"] | 0;
  const int y = it["y"] | 0;
  const int w = it["w"] | W;
  const lv_font_t *font = fontOf(it["font"] | 12);
  const uint32_t color = colorOf(it["color"] | "#ffffff");
  const char *align = it["align"] | "center";
  const bool wrap = it["wrap"] | false;

  lv_obj_t *host = par;
  if (wrap) host = bare(par, x, y, w, it["h"] | 40);

  lv_obj_t *l = lv_label_create(host);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  lv_obj_set_style_text_align(l,
                              strcmp(align, "left") == 0    ? LV_TEXT_ALIGN_LEFT
                              : strcmp(align, "right") == 0 ? LV_TEXT_ALIGN_RIGHT
                                                            : LV_TEXT_ALIGN_CENTER,
                              0);

  if (wrap) {
    lv_obj_set_style_text_line_space(l, 2, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, w);
    lv_obj_set_height(l, LV_SIZE_CONTENT);
    lv_obj_set_pos(l, 0, 0);
  } else if (it["scroll"] | false) {
    lv_label_set_long_mode(l, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_anim_duration(l, 6000, 0);
    lv_obj_set_size(l, w, lv_font_get_line_height(font) + 2);
    lv_obj_set_pos(l, x, y);
  } else {
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_size(l, w, lv_font_get_line_height(font) + 2);
    lv_obj_set_pos(l, x, y);
  }

  const char *bind = it["bind"] | "";
  if (bind[0]) {
    lv_label_set_text(l, "--");
    addBind(l, BIND_LABEL, bind, it["fmt"] | "", 0, 0);
  } else {
    lv_label_set_text(l, it["text"] | "");
  }
}

static void buildPlate(lv_obj_t *par, JsonObjectConst it) {
  const char *style = it["style"] | "glass";
  if (strcmp(style, "none") == 0) return;

  const uint32_t color = colorOf(it["color"] | "#ffffff");
  lv_obj_t *o = bare(par, it["x"] | 0, it["y"] | 0, it["w"] | 10, it["h"] | 10);
  lv_obj_set_style_radius(o, 12, 0);

  if (strcmp(style, "solid") == 0) {
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    return;
  }

  if (strcmp(style, "outline") == 0) {
    lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 1, 0);
    return;
  }

  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_grad_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_bg_main_opa(o, 34, 0);
  lv_obj_set_style_bg_grad_opa(o, 12, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_border_opa(o, 60, 0);
  lv_obj_set_style_border_width(o, 1, 0);
}

static void buildRule(lv_obj_t *par, JsonObjectConst it) {
  lv_obj_t *o = bare(par, it["x"] | 0, it["y"] | 0, it["w"] | 1, it["h"] | 1);
  lv_obj_set_style_bg_color(o, lv_color_hex(colorOf(it["color"] | "#333333")), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

static void buildArc(lv_obj_t *par, JsonObjectConst it) {
  const int size = it["size"] | 60;
  lv_obj_t *arc = lv_arc_create(par);
  lv_obj_remove_style(arc, nullptr, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(arc, size, size);
  lv_obj_set_pos(arc, it["x"] | 0, it["y"] | 0);
  lv_arc_set_rotation(arc, 135);
  lv_arc_set_bg_angles(arc, 0, 270);
  lv_arc_set_range(arc, 0, 100);
  lv_arc_set_value(arc, 0);
  lv_obj_set_style_arc_width(arc, it["width"] | 6, 0);
  lv_obj_set_style_arc_width(arc, it["width"] | 6, LV_PART_INDICATOR);
  lv_obj_set_style_arc_color(arc, lv_color_hex(colorOf(it["track"] | "#333333")), 0);
  lv_obj_set_style_arc_color(arc, lv_color_hex(colorOf(it["color"] | "#38bdf8")),
                             LV_PART_INDICATOR);

  const char *bind = it["bind"] | "";
  if (bind[0]) {
    addBind(arc, BIND_ARC, bind, "", it["min"] | 0.0f, it["max"] | 100.0f);
  } else {
    lv_arc_set_value(arc, it["value"] | 0);
  }
}

static void buildNeedle(lv_obj_t *par, JsonObjectConst it) {
  const int size = it["size"] | 60;
  const int x = it["x"] | 0;
  const int y = it["y"] | 0;

  lv_obj_t *ring = bare(par, x, y, size, size);
  lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_border_color(ring, lv_color_hex(colorOf(it["track"] | "#333333")), 0);
  lv_obj_set_style_border_opa(ring, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ring, 1, 0);

  lv_obj_t *needle = bare(par, x + size / 2 - 2, y + 6, 4, size / 2 - 6);
  lv_obj_set_style_radius(needle, 2, 0);
  lv_obj_set_style_bg_color(needle, lv_color_hex(colorOf(it["color"] | "#38bdf8")), 0);
  lv_obj_set_style_bg_opa(needle, LV_OPA_COVER, 0);
  lv_obj_set_style_transform_pivot_x(needle, 2, 0);
  lv_obj_set_style_transform_pivot_y(needle, size / 2 - 6, 0);

  addBind(needle, BIND_NEEDLE, it["bind"] | "", "", 0, 360);
}

static void buildBar(lv_obj_t *par, JsonObjectConst it) {
  const int w = it["w"] | 60;
  const int h = it["h"] | 6;
  lv_obj_t *bar = lv_bar_create(par);
  lv_obj_set_size(bar, w, h);
  lv_obj_set_pos(bar, it["x"] | 0, it["y"] | 0);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  lv_obj_set_style_radius(bar, h / 2, 0);
  lv_obj_set_style_radius(bar, h / 2, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, lv_color_hex(colorOf(it["track"] | "#333333")), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(colorOf(it["color"] | "#38bdf8")),
                            LV_PART_INDICATOR);

  const char *bind = it["bind"] | "";
  if (bind[0]) {
    addBind(bar, BIND_BAR, bind, "", it["min"] | 0.0f, it["max"] | 100.0f);
  } else {
    lv_bar_set_value(bar, it["value"] | 0, LV_ANIM_OFF);
  }
}

static void buildChart(lv_obj_t *par, JsonObjectConst it) {
  if (sSeriesCount >= 2) return;

  lv_obj_t *chart = lv_chart_create(par);
  lv_obj_remove_style_all(chart);
  lv_obj_set_pos(chart, it["x"] | 0, it["y"] | 0);
  lv_obj_set_size(chart, it["w"] | 100, it["h"] | 40);
  lv_chart_set_type(chart, LV_CHART_TYPE_BAR);
  lv_chart_set_div_line_count(chart, 0, 0);
  lv_obj_set_style_pad_column(chart, 1, 0);
  lv_obj_set_style_size(chart, 0, 0, LV_PART_INDICATOR);

  const uint32_t color = colorOf(it["color"] | "#38bdf8");
  sSeries[sSeriesCount] =
      lv_chart_add_series(chart, lv_color_hex(color), LV_CHART_AXIS_PRIMARY_Y);
  ++sSeriesCount;

  addBind(chart, BIND_CHART, it["bind"] | "", "", 0, 0);
}

static void buildIcon(lv_obj_t *par, JsonObjectConst it) {
  const int size = it["size"] | 32;
  const int x = it["x"] | 0;
  const int y = it["y"] | 0;
  const uint32_t warm = colorOf(it["color"] | "#fbbf24");
  const uint32_t cool = colorOf(it["color2"] | "#b6c4da");

  lv_obj_t *host = bare(par, x, y, size, size);

  lv_obj_t *disc = bare(host, size / 6, size / 6, size / 2, size / 2);
  lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(disc, lv_color_hex(warm), 0);
  lv_obj_set_style_bg_opa(disc, LV_OPA_COVER, 0);

  lv_obj_t *cloud = bare(host, size / 5, size / 2, size * 3 / 5, size / 4);
  lv_obj_set_style_radius(cloud, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(cloud, lv_color_hex(cool), 0);
  lv_obj_set_style_bg_opa(cloud, LV_OPA_TRANSP, 0);

  addBind(cloud, BIND_ICON, it["bind"] | "", "", 0, 0);
}

static void buildMoon(lv_obj_t *par, JsonObjectConst it) {
  const int size = it["size"] | 40;
  lv_obj_t *disc = bare(par, it["x"] | 0, it["y"] | 0, size, size);
  lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(disc, lv_color_hex(colorOf(it["color"] | "#ffffff")), 0);
  lv_obj_set_style_bg_opa(disc, 70, 0);

  lv_obj_t *lit = bare(disc, 0, 0, size / 2, size);
  lv_obj_set_style_radius(lit, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(lit, lv_color_hex(colorOf(it["color"] | "#ffffff")), 0);
  lv_obj_set_style_bg_opa(lit, LV_OPA_COVER, 0);

  addBind(lit, BIND_BAR, "moon.illum", "", 0, 100);
}

void renderClear() {
  sBindCount = 0;
  sSeriesCount = 0;
}

void renderPage(lv_obj_t *parent, JsonObjectConst page) {
  renderClear();
  lv_obj_clean(parent);
  buildBackground(parent, page["bg"]);

  for (JsonObjectConst it : page["items"].as<JsonArrayConst>()) {
    const char *type = it["t"] | "";
    if (strcmp(type, "label") == 0) {
      buildLabel(parent, it);
    } else if (strcmp(type, "plate") == 0) {
      buildPlate(parent, it);
    } else if (strcmp(type, "rule") == 0) {
      buildRule(parent, it);
    } else if (strcmp(type, "arc") == 0) {
      buildArc(parent, it);
    } else if (strcmp(type, "bar") == 0) {
      buildBar(parent, it);
    } else if (strcmp(type, "chart") == 0) {
      buildChart(parent, it);
    } else if (strcmp(type, "icon") == 0) {
      buildIcon(parent, it);
    } else if (strcmp(type, "moon") == 0) {
      buildMoon(parent, it);
    } else if (strcmp(type, "needle") == 0) {
      buildNeedle(parent, it);
    }
  }

  renderRefresh();
}

static void refreshChart(const Bind &b, int index) {
  JsonArrayConst values = dataAt(b.path).as<JsonArrayConst>();
  const int count = values.size();
  if (count <= 0) return;

  float lo = 1e9f;
  float hi = -1e9f;
  for (JsonVariantConst v : values) {
    const float f = v.as<float>();
    if (f < lo) lo = f;
    if (f > hi) hi = f;
  }
  if (hi - lo < 1.0f) hi = lo + 1.0f;

  lv_chart_set_point_count(b.obj, count);
  lv_chart_set_range(b.obj, LV_CHART_AXIS_PRIMARY_Y, (int)floorf(lo), (int)ceilf(hi));

  int i = 0;
  for (JsonVariantConst v : values) {
    lv_chart_set_value_by_id(b.obj, sSeries[index], i++, (int)roundf(v.as<float>()));
  }
}

void renderRefresh() {
  int chartIndex = 0;

  for (int i = 0; i < sBindCount; ++i) {
    const Bind &b = sBinds[i];

    switch (b.kind) {
      case BIND_LABEL: {
        char buf[160];
        dataFormat(b.path, b.fmt, buf, sizeof(buf));
        lv_label_set_text(b.obj, buf);
        if (lv_label_get_long_mode(b.obj) == LV_LABEL_LONG_WRAP) scrollWrapped(b.obj);
        break;
      }
      case BIND_ARC: {
        const float v = dataAt(b.path).as<float>();
        const float span = b.max - b.min > 0 ? b.max - b.min : 1;
        int pct = (int)((v - b.min) * 100.0f / span);
        pct = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
        lv_arc_set_value(b.obj, pct);
        break;
      }
      case BIND_BAR: {
        const float v = dataAt(b.path).as<float>();
        const float span = b.max - b.min > 0 ? b.max - b.min : 1;
        int pct = (int)((v - b.min) * 100.0f / span);
        pct = pct < 0 ? 0 : (pct > 100 ? 100 : pct);
        if (lv_obj_check_type(b.obj, &lv_bar_class)) {
          lv_bar_set_value(b.obj, pct, LV_ANIM_OFF);
        } else {
          const int full = lv_obj_get_width(lv_obj_get_parent(b.obj));
          lv_obj_set_width(b.obj, full * pct / 100);
        }
        break;
      }
      case BIND_CHART:
        refreshChart(b, chartIndex++);
        break;
      case BIND_ICON: {
        const int code = dataAt(b.path).as<int>();
        const bool cloudy = code >= 2;
        lv_obj_set_style_bg_opa(b.obj, cloudy ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        break;
      }
      case BIND_NEEDLE: {
        const int deg = dataAt(b.path).as<int>() % 360;
        lv_obj_set_style_transform_rotation(b.obj, deg * 10, 0);
        break;
      }
    }
  }
}
