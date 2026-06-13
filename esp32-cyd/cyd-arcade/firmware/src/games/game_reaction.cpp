#include "game_play.h"
#include "game_catalog.h"
#include "game_input.h"
#include "score_store.h"
#include "ui_theme.h"
#include "display.h"
#include "hw_config.h"
#include <Arduino.h>

#define COL_BG    ui_rgb565(0x0B0F1A)
#define COL_WAIT  ui_rgb565(0x1F2937)
#define COL_GO    ui_rgb565(0x34D399)
#define COL_EARLY ui_rgb565(0xF87171)

static int best_ms;
static int state;
static uint32_t state_ms;
static char result_msg[24];

enum { ST_IDLE = 0, ST_WAIT, ST_GO, ST_RESULT };

static void draw_scene(const char* msg, uint16_t bg, uint16_t fg) {
    game_play_clear(bg);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(fg, bg);
    tft.drawString(msg, SCREEN_CX, PLAY_Y + PLAY_H / 2 - 10, 2);
}

static void reaction_redraw() {
    switch (state) {
    case ST_WAIT:
        draw_scene("Espere...", COL_WAIT, ui_theme_get()->text_mute);
        break;
    case ST_GO:
        draw_scene("TOQUE!", COL_GO, ui_rgb565(0x0B0F1A));
        break;
    case ST_RESULT:
        draw_scene(result_msg, COL_BG, ui_theme_get()->accent_hi);
        break;
    default:
        draw_scene("Toque p/ iniciar", COL_BG, ui_theme_get()->text_hi);
        break;
    }
}

void game_reaction_run(const GameEntry* cfg) {
    (void)cfg;
    GameHud* hud = game_hud_begin("Reaction", "reaction", 0x34D399);
    if (!hud) return;

    best_ms = 0;
    state = ST_IDLE;
    game_hud_set_score(hud, 0);
    draw_scene("Toque p/ iniciar", COL_BG, ui_theme_get()->text_hi);

    GameInput in;
    for (;;) {
        game_frame_tick();
        game_input_poll(&in);
        if (game_hud_poll(hud)) {
            if (hud->score > 0) score_store_save(hud->engine, hud->score);
            game_hud_end(hud);
            return;
        }
        if (game_hud_consume_resume_redraw(hud))
            reaction_redraw();

        if (state == ST_IDLE && in.just_pressed && in.y >= PLAY_Y) {
            state = ST_WAIT;
            state_ms = millis() + random(1200, 3500);
            draw_scene("Espere...", COL_WAIT, ui_theme_get()->text_mute);
        } else if (state == ST_WAIT) {
            if (in.just_pressed && in.y >= PLAY_Y) {
                state = ST_IDLE;
                draw_scene("Cedo demais!", COL_EARLY, ui_theme_get()->text_hi);
                delay(800);
                draw_scene("Toque p/ iniciar", COL_BG, ui_theme_get()->text_hi);
            } else if (millis() >= state_ms) {
                state = ST_GO;
                state_ms = millis();
                draw_scene("TOQUE!", COL_GO, ui_rgb565(0x0B0F1A));
            }
        } else if (state == ST_GO && in.just_pressed && in.y >= PLAY_Y) {
            const int ms = (int)(millis() - state_ms);
            state = ST_RESULT;
            snprintf(result_msg, sizeof(result_msg), "%d ms", ms);
            draw_scene(result_msg, COL_BG, ui_theme_get()->accent_hi);
            if (best_ms == 0 || ms < best_ms) {
                best_ms = ms;
                const int pts = max(1, 1000 - ms);
                game_hud_set_score(hud, pts);
            }
            delay(1200);
            state = ST_IDLE;
            draw_scene("Toque p/ iniciar", COL_BG, ui_theme_get()->text_hi);
        } else if (state == ST_RESULT) {
            /* aguarda delay */
        }

        game_frame_delay();
    }
}
