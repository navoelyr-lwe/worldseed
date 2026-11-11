/* Worldseed Engine — in-world HUD (P11).
 *
 * The HUD draws the observer's read-outs over the rendered frame: a status bar
 * (world clock, tick, speed, view, population, frame rate), a live inspection
 * panel for the selected animal, a world map of the loaded region set, an event
 * timeline of the world's real events, and a help overlay. Everything is drawn
 * with the built-in C17 font (src/ui/ws_font) into the presentation framebuffer;
 * nothing here touches the simulation, the scene or the world (docs/00
 * "gözlemci salt okunurdur").
 */
#ifndef WS_UI_HUD_H
#define WS_UI_HUD_H

#include "../render/ws_framebuffer.h"
#include "ws_session.h"

typedef struct ws_hud_options {
    int show_panels; /* info panel + timeline (on by default) */
    int show_help;   /* help overlay */
    int show_stats;  /* population/statistics panel */
    int show_settings; /* persisted-settings panel */
    int scale;       /* base font scale (1 = small, 2 = large) */
} ws_hud_options;

ws_hud_options ws_hud_options_default(void);

/* Draw the whole HUD for the current session state into `fb`. */
void ws_hud_draw(ws_framebuffer *fb, const ws_session *s, const ws_hud_options *opts);

/* Mouse hit-testing for the modal menus (docs/17 item 9 / "Ana deneyim").
 *
 * A click must land on the row the user sees, so these use exactly the layout
 * ws_hud_draw paints. `fb_w`/`fb_h` are the framebuffer dimensions the HUD was
 * drawn at and `scale` the text scale it used; `(px, py)` is the pointer in
 * framebuffer pixels. Each returns the row id under the point (a
 * `ws_settings_item` or `ws_world_choice`), or -1 when the point is not on a
 * menu row. */
int ws_hud_settings_row_at(uint32_t fb_w, uint32_t fb_h, int scale, int px, int py);
int ws_hud_world_select_row_at(uint32_t fb_w, uint32_t fb_h, int scale, int px, int py);

#endif /* WS_UI_HUD_H */
