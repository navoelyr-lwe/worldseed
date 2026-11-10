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
    int scale;       /* base font scale (1 = small, 2 = large) */
} ws_hud_options;

ws_hud_options ws_hud_options_default(void);

/* Draw the whole HUD for the current session state into `fb`. */
void ws_hud_draw(ws_framebuffer *fb, const ws_session *s, const ws_hud_options *opts);

#endif /* WS_UI_HUD_H */
