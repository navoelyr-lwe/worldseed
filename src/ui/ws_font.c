/* Worldseed Engine — built-in bitmap font and HUD text drawing (P11). */
#include "ws_font.h"

#include "ws_font_data.h"

#include <string.h>

uint32_t ws_font_advance(int scale)
{
    if (scale < 1) {
        scale = 1;
    }
    return (WS_FONT_GLYPH_W + 1u) * (uint32_t)scale;
}

static int glyph_rows(const char *art, uint8_t rows[WS_FONT_GLYPH_H][WS_FONT_GLYPH_W])
{
    for (uint32_t r = 0; r < WS_FONT_GLYPH_H; ++r) {
        for (uint32_t c = 0; c < WS_FONT_GLYPH_W; ++c) {
            rows[r][c] = (uint8_t)(art[r * WS_FONT_GLYPH_W + c] == '#');
        }
    }
    return 1;
}

static const uint8_t *glyph_art(unsigned char ch)
{
    if (ch < WS_FONT_FIRST || ch > WS_FONT_LAST) {
        ch = '?';
    }
    return (const uint8_t *)WS_FONT_GLYPHS[ch - WS_FONT_FIRST];
}

void ws_font_fill_rect(ws_framebuffer *fb, int x, int y, uint32_t w, uint32_t h,
                       const uint8_t color[4])
{
    if (fb == NULL || fb->pixels == NULL || color == NULL) {
        return;
    }
    for (uint32_t dy = 0; dy < h; ++dy) {
        int py = y + (int)dy;
        if (py < 0 || (uint32_t)py >= fb->height) {
            continue;
        }
        for (uint32_t dx = 0; dx < w; ++dx) {
            int px = x + (int)dx;
            if (px < 0 || (uint32_t)px >= fb->width) {
                continue;
            }
            uint8_t *p = &fb->pixels[((size_t)py * fb->width + (size_t)px) * 4u];
            p[0] = color[0];
            p[1] = color[1];
            p[2] = color[2];
            p[3] = color[3];
        }
    }
}

void ws_font_stroke_rect(ws_framebuffer *fb, int x, int y, uint32_t w, uint32_t h,
                         int scale, const uint8_t color[4])
{
    if (scale < 1) {
        scale = 1;
    }
    uint32_t t = (uint32_t)scale;
    ws_font_fill_rect(fb, x, y, w, t, color);                    /* top */
    ws_font_fill_rect(fb, x, y + (int)(h - t), w, t, color);     /* bottom */
    ws_font_fill_rect(fb, x, y, t, h, color);                    /* left */
    ws_font_fill_rect(fb, x + (int)(w - t), y, t, h, color);     /* right */
}

void ws_font_draw(ws_framebuffer *fb, int x, int y, const char *text, int scale,
                  const uint8_t color[4])
{
    if (fb == NULL || text == NULL || color == NULL) {
        return;
    }
    if (scale < 1) {
        scale = 1;
    }
    uint32_t adv = ws_font_advance(scale);
    int pen = x;
    for (const unsigned char *s = (const unsigned char *)text; *s != '\0'; ++s) {
        if (*s == '\n') {
            pen = x;
            y += (int)(WS_FONT_GLYPH_H * (uint32_t)scale);
            continue;
        }
        uint8_t rows[WS_FONT_GLYPH_H][WS_FONT_GLYPH_W];
        glyph_rows((const char *)glyph_art(*s), rows);
        for (uint32_t r = 0; r < WS_FONT_GLYPH_H; ++r) {
            for (uint32_t c = 0; c < WS_FONT_GLYPH_W; ++c) {
                if (!rows[r][c]) {
                    continue;
                }
                ws_font_fill_rect(fb, pen + (int)(c * (uint32_t)scale),
                                  y + (int)(r * (uint32_t)scale),
                                  (uint32_t)scale, (uint32_t)scale, color);
            }
        }
        pen += (int)adv;
    }
}

void ws_font_measure(const char *text, int scale, uint32_t *out_w, uint32_t *out_h)
{
    if (scale < 1) {
        scale = 1;
    }
    uint32_t adv = ws_font_advance(scale);
    uint32_t lines = 1u;
    uint32_t longest = 0u;
    uint32_t cur = 0u;
    if (text != NULL) {
        for (const char *s = text; *s != '\0'; ++s) {
            if (*s == '\n') {
                if (cur > longest) {
                    longest = cur;
                }
                cur = 0u;
                lines += 1u;
            } else {
                cur += adv;
            }
        }
    }
    if (cur > longest) {
        longest = cur;
    }
    if (out_w != NULL) {
        /* Drop the trailing 1 px spacing so the width is the ink extent. */
        *out_w = longest > 0u ? longest - (uint32_t)scale : 0u;
    }
    if (out_h != NULL) {
        *out_h = lines * WS_FONT_GLYPH_H * (uint32_t)scale +
                 (lines > 1u ? (lines - 1u) * (uint32_t)scale : 0u);
    }
}

void ws_font_draw_centered(ws_framebuffer *fb, int box_x, int box_y, uint32_t box_w,
                           const char *text, int scale, const uint8_t color[4])
{
    uint32_t w = 0u;
    ws_font_measure(text, scale, &w, NULL);
    int x = box_x + (int)((box_w > w ? box_w - w : 0u) / 2u);
    ws_font_draw(fb, x, box_y, text, scale, color);
}
