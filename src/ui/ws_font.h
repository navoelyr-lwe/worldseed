/* Worldseed Engine — built-in bitmap font and HUD text drawing (P11).
 *
 * The user interface needs to draw labels, numbers and short status lines into
 * the framebuffer. There is no font asset and no third-party text library
 * (ADR-008): every glyph is a 5x7 bitmap defined in this file as our own data.
 * A glyph covers ASCII 32..126; drawing scales by an integer factor so the same
 * face serves a small status bar and a large title.
 *
 * Text is drawn straight into the RGBA8 framebuffer, which is presentation only:
 * the UI layer never writes to the world or the render scene (docs/00 "gözlemci
 * salt okunurdur").
 */
#ifndef WS_UI_FONT_H
#define WS_UI_FONT_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "../render/ws_framebuffer.h"

#define WS_FONT_FIRST 32u  /* first glyph code (space) */
#define WS_FONT_LAST 126u  /* last glyph code ('~') */
#define WS_FONT_GLYPH_W 5u /* glyph width in pixels at scale 1 */
#define WS_FONT_GLYPH_H 7u /* glyph height in pixels at scale 1 */

/* Text metrics for a string at the given scale, in pixels. */
void ws_font_measure(const char *text, int scale, uint32_t *out_w, uint32_t *out_h);

/* Advance width of one glyph cell at `scale` (glyph + 1 px spacing). */
uint32_t ws_font_advance(int scale);

/* Draw `text` with its top-left corner at (x, y) at integer `scale` (>= 1).
 * `color` is RGBA. Clipping is per-pixel, so off-screen text is safe. */
void ws_font_draw(ws_framebuffer *fb, int x, int y, const char *text, int scale,
                  const uint8_t color[4]);

/* Draw `text` centred horizontally in a box of width `box_w` starting at
 * `box_x`, top at `box_y`. */
void ws_font_draw_centered(ws_framebuffer *fb, int box_x, int box_y, uint32_t box_w,
                           const char *text, int scale, const uint8_t color[4]);

/* Fill an axis-aligned rectangle with `color`, clipped to the framebuffer. */
void ws_font_fill_rect(ws_framebuffer *fb, int x, int y, uint32_t w, uint32_t h,
                       const uint8_t color[4]);

/* Draw a `scale`-thick rectangle outline. */
void ws_font_stroke_rect(ws_framebuffer *fb, int x, int y, uint32_t w, uint32_t h,
                         int scale, const uint8_t color[4]);

#endif /* WS_UI_FONT_H */
