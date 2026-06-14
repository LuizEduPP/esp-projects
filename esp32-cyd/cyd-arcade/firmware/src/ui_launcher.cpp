#include "ui_launcher.h"
#include "display.h"
#include "hw_config.h"
#include "score_store.h"
#include "touch_input.h"
#include "ui_draw.h"
#include "ui_icons.h"
#include "ui_theme.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#define TH ui_theme_get()
#define SCROLL_TAP_PX    10
#define LIST_VIEW_BOT    (UI_LIST_TOP + UI_LIST_VIEW_H)
#define SCROLL_PAINT_MS  28
#define SCROLL_PAINT_DY  10

static void label_fit(const char* src, char* out, size_t out_sz, int max_w) {
    strncpy(out, src, out_sz - 1);
    out[out_sz - 1] = '\0';
    if ((int)tft.textWidth(out, 1) <= max_w) return;
    size_t n = strlen(out);
    while (n > 0 && (int)tft.textWidth(out, 1) + 12 > max_w)
        out[--n] = '\0';
    if (n > 0 && n + 3 < out_sz) {
        out[n] = '.';
        out[n + 1] = '.';
        out[n + 2] = '.';
        out[n + 3] = '\0';
    }
}

static int list_content_h(int count) {
    if (count <= 0) return 0;
    return count * UI_LIST_ROW_STEP - UI_LIST_GAP;
}

static int list_max_scroll(int count) {
    const int max_s = list_content_h(count) - UI_LIST_VIEW_H;
    return max_s > 0 ? max_s : 0;
}

static void draw_scrollbar(int count, int scroll_y) {
    const int max_s = list_max_scroll(count);
    const int track_y = UI_LIST_TOP + 4;
    const int track_h = UI_LIST_VIEW_H - 8;
    const int bar_x = UI_SCROLLBAR_X;

    tft.fillRoundRect(bar_x, track_y, UI_SCROLLBAR_W, track_h, 3, TH->card);
    if (max_s <= 0) return;

    const int content_h = list_content_h(count);
    int thumb_h = track_h * UI_LIST_VIEW_H / content_h;
    if (thumb_h < 16) thumb_h = 16;
    if (thumb_h > track_h) thumb_h = track_h;

    int thumb_y = track_y;
    if (max_s > 0 && track_h > thumb_h)
        thumb_y += (scroll_y * (track_h - thumb_h)) / max_s;

    tft.fillRoundRect(bar_x, thumb_y, UI_SCROLLBAR_W, thumb_h, 3, TH->accent);
}

static void repair_header_gap() {
    tft.fillRect(0, UI_HDR_H, SCREEN_W, UI_LIST_TOP - UI_HDR_H, TH->bg);
}

static void list_viewport_begin() {
    tft.setViewport(0, UI_LIST_TOP, SCREEN_W, UI_LIST_VIEW_H);
}

static void list_viewport_end(bool repair_gap) {
    tft.resetViewport();
    if (repair_gap)
        repair_header_gap();
}

void launcher_draw_header(int count) {
    char sub[24];
    snprintf(sub, sizeof(sub), "%d jogos", count);
    ui_draw_app_header_btn(UI_ICON_GRID, "Arcade", sub, UI_ICON_GEAR, UI_HDR_GEAR_X);
    repair_header_gap();
}

/* cy relativo ao topo da lista (UI_LIST_TOP) */
static void draw_list_row_content(const GameEntry* game, int cx, int cy, bool sel, int idx) {
    (void)idx;
    if (cy + UI_LIST_ROW_H <= 0 || cy >= UI_LIST_VIEW_H) return;

    const uint16_t bg = TH->card;
    tft.fillRoundRect(cx, cy, UI_LIST_ROW_W, UI_LIST_ROW_H, UI_CARD_R, bg);
    if (sel)
        tft.drawRoundRect(cx, cy, UI_LIST_ROW_W, UI_LIST_ROW_H, UI_CARD_R, TH->accent);

    tft.fillRoundRect(cx + 8, cy + 6, UI_LIST_STRIPE_W, UI_LIST_ROW_H - 12, 2,
                      ui_theme_game_color(game->color));

    char title[24];
    label_fit(game->title, title, sizeof(title), UI_LIST_ROW_W - 96);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(TH->text_hi, bg);
    tft.drawString(title, cx + 8 + UI_LIST_STRIPE_W + 10, cy + UI_LIST_ROW_H / 2, 2);

    const int best = score_store_get(game->engine);
    if (best > 0) {
        char holder[SCORE_NAME_LEN + 1];
        score_store_get_name(game->engine, holder, sizeof(holder));
        char rec[24];
        if (holder[0])
            snprintf(rec, sizeof(rec), "%s %d", holder, best);
        else
            snprintf(rec, sizeof(rec), "%d", best);
        label_fit(rec, rec, sizeof(rec), 64);
        const int badge_w = (int)tft.textWidth(rec, 1) + 12;
        const int badge_x = cx + UI_LIST_ROW_W - badge_w - 8;
        const int badge_y = cy + (UI_LIST_ROW_H - 20) / 2;
        tft.fillRoundRect(badge_x, badge_y, badge_w, 20, 4, TH->surface);
        tft.drawRoundRect(badge_x, badge_y, badge_w, 20, 4, TH->border);
        tft.setTextColor(TH->accent, TH->surface);
        tft.setTextDatum(MC_DATUM);
        tft.drawString(rec, badge_x + badge_w / 2, badge_y + 10, 1);
    }
}

static void draw_list_row_at(GameEntry* games, int count, int scroll_y, int idx, int sel) {
    if (idx < 0 || idx >= count) return;
    const int cy = idx * UI_LIST_ROW_STEP - scroll_y;
    if (cy + UI_LIST_ROW_H <= 0 || cy >= UI_LIST_VIEW_H) return;

    tft.startWrite();
    list_viewport_begin();
    tft.fillRect(UI_PAD, cy, UI_LIST_ROW_W, UI_LIST_ROW_STEP, TH->bg);
    draw_list_row_content(&games[idx], UI_PAD, cy, idx == sel, idx);
    list_viewport_end(true);
    tft.endWrite();
}

static void paint_list_rows(GameEntry* games, int count, int scroll_y, int sel) {
    const int first = scroll_y / UI_LIST_ROW_STEP;
    const int last = (scroll_y + UI_LIST_VIEW_H) / UI_LIST_ROW_STEP + 1;
    for (int i = first; i <= last && i < count; i++) {
        if (i < 0) continue;
        const int cy = i * UI_LIST_ROW_STEP - scroll_y;
        draw_list_row_content(&games[i], UI_PAD, cy, i == sel, i);
    }
}

static void draw_list_viewport(GameEntry* games, int count, int scroll_y, int sel, bool repair_gap) {
    tft.startWrite();
    list_viewport_begin();
    tft.fillRect(0, 0, SCREEN_W, UI_LIST_VIEW_H, TH->bg);
    paint_list_rows(games, count, scroll_y, sel);
    list_viewport_end(repair_gap);
    draw_scrollbar(count, scroll_y);
    tft.endWrite();
}

static void paint_list_scroll(GameEntry* games, int count, int scroll_y, int sel, int old_scroll) {
    if (old_scroll < 0 || scroll_y == old_scroll) {
        draw_list_viewport(games, count, scroll_y, sel, false);
        return;
    }

    tft.startWrite();
    list_viewport_begin();

    const int min_scroll = scroll_y < old_scroll ? scroll_y : old_scroll;
    const int max_scroll = scroll_y > old_scroll ? scroll_y : old_scroll;
    const int first = min_scroll / UI_LIST_ROW_STEP - 1;
    const int last = (max_scroll + UI_LIST_VIEW_H) / UI_LIST_ROW_STEP + 2;

    for (int i = first; i <= last && i < count; i++) {
        if (i < 0) continue;
        const int old_cy = i * UI_LIST_ROW_STEP - old_scroll;
        if (old_cy + UI_LIST_ROW_STEP > 0 && old_cy < UI_LIST_VIEW_H)
            tft.fillRect(0, old_cy, SCREEN_W, UI_LIST_ROW_STEP, TH->bg);
    }

    for (int i = first; i <= last && i < count; i++) {
        if (i < 0) continue;
        const int cy = i * UI_LIST_ROW_STEP - scroll_y;
        if (cy + UI_LIST_ROW_H <= 0 || cy >= UI_LIST_VIEW_H) continue;
        draw_list_row_content(&games[i], UI_PAD, cy, i == sel, i);
    }

    list_viewport_end(false);
    draw_scrollbar(count, scroll_y);
    tft.endWrite();
}

static void redraw_screen(GameEntry* games, int count, int scroll_y, int sel) {
    tft.startWrite();
    tft.fillScreen(TH->bg);
    launcher_draw_header(count);
    list_viewport_begin();
    paint_list_rows(games, count, scroll_y, sel);
    list_viewport_end(true);
    draw_scrollbar(count, scroll_y);
    tft.endWrite();
}

static bool hit_gear(int16_t tx, int16_t ty) {
    return tx >= UI_HDR_GEAR_ZONE_X && tx < UI_HDR_GEAR_ZONE_X + UI_HDR_GEAR_ZONE_W
        && ty >= 0 && ty < UI_HDR_H;
}

static int scroll_y_from_touch(int16_t ty, int count) {
    const int max_s = list_max_scroll(count);
    if (max_s <= 0) return 0;

    const int track_y = UI_LIST_TOP + 4;
    const int track_h = UI_LIST_VIEW_H - 8;
    const int content_h = list_content_h(count);
    int thumb_h = track_h * UI_LIST_VIEW_H / content_h;
    if (thumb_h < 16) thumb_h = 16;
    if (thumb_h > track_h) thumb_h = track_h;

    const int range = track_h - thumb_h;
    if (range <= 0) return 0;

    int rel = ty - track_y - thumb_h / 2;
    if (rel < 0) rel = 0;
    if (rel > range) rel = range;
    return (rel * max_s) / range;
}

static bool hit_scrollbar(int16_t tx, int16_t ty) {
    return tx >= UI_SCROLLBAR_X - 6 && tx < SCREEN_W
        && ty >= UI_LIST_TOP && ty < LIST_VIEW_BOT;
}

static int list_hit(int16_t tx, int16_t ty, int scroll_y, int count) {
    if (ty < UI_LIST_TOP || ty >= LIST_VIEW_BOT) return -1;
    if (tx < UI_PAD || tx >= UI_PAD + UI_LIST_ROW_W) return -1;
    if (tx >= UI_SCROLLBAR_X) return -1;
    const int rel_y = ty - UI_LIST_TOP + scroll_y;
    const int idx = rel_y / UI_LIST_ROW_STEP;
    if (idx < 0 || idx >= count) return -1;
    if (rel_y - idx * UI_LIST_ROW_STEP >= UI_LIST_ROW_H) return -1;
    return idx;
}

int launcher_show(GameEntry* games, int count) {
    int scroll_y = 0;
    int sel = -1;
    int painted_scroll = -1;
    redraw_screen(games, count, scroll_y, sel);
    painted_scroll = scroll_y;

    if (count == 0) {
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TH->text_mute, TH->bg);
        tft.drawString("Nenhum jogo", SCREEN_CX, SCREEN_H / 2, 2);
        while (true) delay(1000);
    }

    touch_wait_release();

    bool was_pressed = false;
    bool cfg_pending = false;
    bool scrolling = false;
    bool scroll_bar_drag = false;
    int16_t last_tx = 0, last_ty = 0;
    int16_t touch_start_ty = 0;
    int scroll_anchor = 0;
    uint32_t last_paint_ms = 0;

    while (true) {
        touch_poll();
        const bool down = touch_is_pressed();
        if (down) {
            last_tx = touch_get_x();
            last_ty = touch_get_y();

            if (!was_pressed) {
                touch_start_ty = last_ty;
                scroll_anchor = scroll_y;
                cfg_pending = hit_gear(last_tx, last_ty);
                scroll_bar_drag = !cfg_pending && hit_scrollbar(last_tx, last_ty);
                if (scroll_bar_drag) {
                    scrolling = true;
                    const int ns = scroll_y_from_touch(last_ty, count);
                    if (ns != scroll_y) {
                        const int prev = scroll_y;
                        scroll_y = ns;
                        paint_list_scroll(games, count, scroll_y, sel, prev);
                        painted_scroll = scroll_y;
                        last_paint_ms = millis();
                    }
                } else if (!cfg_pending) {
                    const int idx = list_hit(last_tx, last_ty, scroll_y, count);
                    if (idx >= 0 && idx != sel) {
                        const int old = sel;
                        sel = idx;
                        if (old >= 0)
                            draw_list_row_at(games, count, scroll_y, old, sel);
                        draw_list_row_at(games, count, scroll_y, sel, sel);
                    }
                }
            } else if (scroll_bar_drag) {
                scrolling = true;
                const int ns = scroll_y_from_touch(last_ty, count);
                if (ns != scroll_y) {
                    scroll_y = ns;
                    const uint32_t now = millis();
                    if (now - last_paint_ms >= SCROLL_PAINT_MS) {
                        const int prev = painted_scroll;
                        paint_list_scroll(games, count, scroll_y, sel, prev);
                        painted_scroll = scroll_y;
                        last_paint_ms = now;
                    }
                }
            } else if (!cfg_pending) {
                const int dy = touch_start_ty - last_ty;
                if (!scrolling && abs(dy) >= SCROLL_TAP_PX)
                    scrolling = true;
                if (scrolling) {
                    const int ns = constrain(scroll_anchor + dy, 0, list_max_scroll(count));
                    if (ns != scroll_y) {
                        scroll_y = ns;
                        const uint32_t now = millis();
                        if (now - last_paint_ms >= SCROLL_PAINT_MS &&
                            abs(scroll_y - painted_scroll) >= SCROLL_PAINT_DY) {
                            const int prev = painted_scroll;
                            paint_list_scroll(games, count, scroll_y, sel, prev);
                            painted_scroll = scroll_y;
                            last_paint_ms = now;
                        }
                    }
                }
            }
        } else if (was_pressed) {
            if (scrolling && scroll_y != painted_scroll) {
                draw_list_viewport(games, count, scroll_y, sel, true);
                painted_scroll = scroll_y;
            }
            if (cfg_pending) {
                return LAUNCHER_SETTINGS;
            } else if (!scrolling && sel >= 0) {
                return sel;
            }
            if (sel >= 0) {
                const int old = sel;
                sel = -1;
                draw_list_row_at(games, count, scroll_y, old, sel);
            }
            cfg_pending = false;
            scrolling = false;
            scroll_bar_drag = false;
        }
        was_pressed = down;
        delay(scrolling && down ? 5 : 10);
    }
}
