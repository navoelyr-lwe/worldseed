/* Worldseed Engine — built-in font tests (P11).
 *
 * The font is our own data (ADR-008), so the tests assert its integrity: every
 * glyph is exactly the documented size, metrics are consistent, and drawing
 * actually deposits ink. No external font is involved.
 */
#include "ws_test.h"

#include "ui/ws_font.h"
#include "ui/ws_font_data.h"

#include <string.h>

static int test_glyph_data_wellformed(void)
{
    /* 95 glyphs for ASCII 32..126, each exactly 7 rows x 5 columns. */
    unsigned glyph_count = WS_FONT_LAST - WS_FONT_FIRST + 1u;
    WS_CHECK(glyph_count == 95u);
    for (unsigned i = 0; i < 95u; ++i) {
        const char *g = WS_FONT_GLYPHS[i];
        WS_CHECK(g != NULL);
        WS_CHECK(strlen(g) == WS_FONT_GLYPH_W * WS_FONT_GLYPH_H);
        for (size_t k = 0; k < strlen(g); ++k) {
            WS_CHECK(g[k] == '#' || g[k] == '.');
        }
    }
    /* The space glyph must be blank. */
    for (size_t k = 0; k < 35u; ++k) {
        WS_CHECK(WS_FONT_GLYPHS[0][k] == '.');
    }
    /* A letter must carry ink (guards against an all-blank table). */
    int ink = 0;
    for (size_t k = 0; k < 35u; ++k) {
        if (WS_FONT_GLYPHS['A' - WS_FONT_FIRST][k] == '#') {
            ink = 1;
        }
    }
    WS_CHECK(ink == 1);
    return 1;
}

static int test_metrics(void)
{
    uint32_t w = 0u, h = 0u;
    ws_font_measure("AB", 1, &w, &h);
    /* Two glyphs: 2*6 px advance minus 1 px trailing spacing = 11; height 7. */
    WS_CHECK(w == 11u);
    WS_CHECK(h == 7u);

    ws_font_measure("AB", 2, &w, &h);
    WS_CHECK(w == 22u);
    WS_CHECK(h == 14u);

    /* A newline starts a second line. */
    ws_font_measure("A\nB", 1, &w, &h);
    WS_CHECK(h == 15u);
    WS_CHECK(ws_font_advance(1) == 6u);
    WS_CHECK(ws_font_advance(3) == 18u);
    return 1;
}

static int test_draw_deposits_ink(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 64u, 16u) == WS_OK);
    ws_framebuffer_clear(&fb, 0u, 0u, 0u, 255u);
    const uint8_t white[4] = {255u, 255u, 255u, 255u};
    ws_font_draw(&fb, 0, 0, "Hi", 1, white);
    size_t lit = 0u;
    for (size_t i = 0; i < (size_t)fb.width * fb.height; ++i) {
        if (fb.pixels[i * 4u] != 0u) {
            lit += 1u;
        }
    }
    WS_CHECK(lit > 0u);
    /* Drawing off-screen must not crash or write out of bounds. */
    ws_font_draw(&fb, -100, -100, "offscreen", 2, white);
    ws_font_draw(&fb, 60, 10, "clipped", 2, white);
    ws_framebuffer_free(&fb);
    return 1;
}

static int test_centered_and_rect(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 200u, 40u) == WS_OK);
    ws_framebuffer_clear(&fb, 0u, 0u, 0u, 255u);
    const uint8_t c[4] = {10u, 200u, 40u, 255u};
    ws_font_draw_centered(&fb, 0, 0, fb.width, "X", 1, c);
    ws_font_fill_rect(&fb, 5, 5, 10u, 10u, c);
    /* The filled rect must set exactly its pixels. */
    WS_CHECK(fb.pixels[(5u * fb.width + 5u) * 4u] == 10u);
    WS_CHECK(fb.pixels[(14u * fb.width + 14u) * 4u] == 10u);
    WS_CHECK(fb.pixels[(4u * fb.width + 4u) * 4u] == 0u);
    ws_framebuffer_free(&fb);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"font_glyph_data_wellformed", test_glyph_data_wellformed},
        {"font_metrics", test_metrics},
        {"font_draw_deposits_ink", test_draw_deposits_ink},
        {"font_centered_and_rect", test_centered_and_rect},
    };
    return ws_test_run("font", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
