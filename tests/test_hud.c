/* Worldseed Engine — HUD tests (P11).
 *
 * The HUD is a read-only overlay: it must draw over the rendered world without
 * touching the simulation, and it must be deterministic. These tests check that
 * drawing changes the frame (panels are actually painted), that drawing never
 * changes the world digest, and that the help/map overlays are safe to toggle.
 */
#include "ws_test.h"

#include "render/ws_framebuffer.h"
#include "ui/ws_hud.h"
#include "ui/ws_session.h"

#include <string.h>

static ws_session *make(uint64_t seed)
{
    ws_session_config cfg = ws_session_config_default(seed);
    ws_session *s = NULL;
    if (ws_session_create(&s, &cfg) != WS_OK) {
        return NULL;
    }
    return s;
}

static void run_seconds(ws_session *s, double seconds)
{
    for (double t = 0.0; t < seconds; t += 0.05) {
        (void)ws_session_step(s, 0.05);
    }
}

static uint64_t frame_hash(const ws_framebuffer *fb)
{
    uint64_t h = 1469598103934665603ULL;
    size_t n = (size_t)fb->width * fb->height * 4u;
    for (size_t i = 0; i < n; ++i) {
        h ^= fb->pixels[i];
        h *= 1099511628211ULL;
    }
    return h;
}

static int test_hud_draws(void)
{
    ws_session *s = make(100ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 2.0);
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 320u, 180u) == WS_OK);
    WS_CHECK(ws_session_render(s, &fb, NULL) == WS_OK);
    uint64_t world_only = frame_hash(&fb);
    ws_hud_options o = ws_hud_options_default();
    ws_hud_draw(&fb, s, &o);
    uint64_t with_hud = frame_hash(&fb);
    /* The HUD paints panels and text, so the frame changes. */
    WS_CHECK(world_only != with_hud);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return 1;
}

static int test_hud_is_readonly(void)
{
    /* Drawing the HUD on two sessions with the same seed and tick count must
     * leave both world digests unchanged and equal. */
    ws_session *a = make(101ULL);
    ws_session *b = make(101ULL);
    WS_CHECK(a != NULL && b != NULL);
    run_seconds(a, 3.0);
    run_seconds(b, 3.0);
    char before[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(a, before) == WS_OK);

    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 256u, 144u) == WS_OK);
    ws_hud_options o = ws_hud_options_default();
    o.show_help = 1;
    for (int i = 0; i < 3; ++i) {
        WS_CHECK(ws_session_render(b, &fb, NULL) == WS_OK);
        ws_hud_draw(&fb, b, &o);
    }
    char after[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(b, after) == WS_OK);
    WS_CHECK(strcmp(before, after) == 0);
    ws_framebuffer_free(&fb);
    ws_session_destroy(a);
    ws_session_destroy(b);
    return 1;
}

static int test_hud_map_overlay(void)
{
    ws_session *s = make(102ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 1.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_MAP, 0.0);
    WS_CHECK(ws_session_view_mode(s) == WS_VIEW_MAP);
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 256u, 144u) == WS_OK);
    WS_CHECK(ws_session_render(s, &fb, NULL) == WS_OK);
    ws_hud_options o = ws_hud_options_default();
    ws_hud_draw(&fb, s, &o); /* must not crash with the map overlay */
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return 1;
}

static int test_hud_scale(void)
{
    ws_session *s = make(103ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 1.0);
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 640u, 360u) == WS_OK);
    WS_CHECK(ws_session_render(s, &fb, NULL) == WS_OK);
    ws_hud_options o = ws_hud_options_default();
    o.scale = 2;
    ws_hud_draw(&fb, s, &o); /* large scale must not overrun the buffer */
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return 1;
}

static int test_hud_null_safe(void)
{
    ws_hud_options o = ws_hud_options_default();
    ws_hud_draw(NULL, NULL, &o); /* must not crash */
    return 1;
}

static int test_hud_stats_panel(void)
{
    ws_session *s = make(104ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 6.0);
    WS_CHECK(ws_session_stats_visible(s) == 0);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_STATS, 0.0);
    WS_CHECK(ws_session_stats_visible(s) == 1);
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 320u, 180u) == WS_OK);
    WS_CHECK(ws_session_render(s, &fb, NULL) == WS_OK);
    ws_hud_options o = ws_hud_options_default();
    ws_hud_draw(&fb, s, &o); /* stats panel must draw without overrunning */
    /* Demography and the population history come from the real ecology. */
    float hist[8u * WS_SPECIES_COUNT];
    uint32_t n = ws_session_population_history(s, hist, 8u);
    WS_CHECK(n <= 8u);
    for (uint32_t i = 0; i < n * WS_SPECIES_COUNT; ++i) {
        WS_CHECK(hist[i] >= 0.0f);
    }
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return 1;
}

static int test_hud_settings_panel(void)
{
    ws_session *s = make(105ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 1.0);
    WS_CHECK(ws_session_settings_visible(s) == 0);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SETTINGS, 0.0);
    WS_CHECK(ws_session_settings_visible(s) == 1);
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 320u, 180u) == WS_OK);
    WS_CHECK(ws_session_render(s, &fb, NULL) == WS_OK);
    ws_hud_options o = ws_hud_options_default();
    ws_hud_draw(&fb, s, &o); /* settings panel must draw without overrunning */
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"hud_draws_over_world", test_hud_draws},
        {"hud_is_readonly", test_hud_is_readonly},
        {"hud_map_overlay", test_hud_map_overlay},
        {"hud_scale", test_hud_scale},
        {"hud_null_safe", test_hud_null_safe},
        {"hud_stats_panel", test_hud_stats_panel},
        {"hud_settings_panel", test_hud_settings_panel},
    };
    return ws_test_run("hud", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
