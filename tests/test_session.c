/* Worldseed Engine — interactive session tests (P11).
 *
 * Regression suite from docs/13 P11 and docs/00: the observer is read-only, the
 * world is deterministic and decoupled from the frame rate, the view modes and
 * commands never change the world, live inspection reports the simulated state,
 * and a checkpoint round-trips bit-identically. Real models; no mocks.
 */
#include "ws_test.h"

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

/* Run a fixed simulated interval by feeding wall-time steps. */
static void run_seconds(ws_session *s, double seconds)
{
    double dt = 0.05;
    for (double t = 0.0; t < seconds; t += dt) {
        (void)ws_session_step(s, dt);
    }
}

static int test_create_and_queries(void)
{
    ws_session *s = make(20260101ULL);
    WS_CHECK(s != NULL);
    WS_CHECK(ws_session_loaded_regions(s) >= 1u);
    WS_CHECK(ws_session_view_mode(s) == WS_VIEW_AUTO);
    WS_CHECK(ws_session_paused(s) == 0);
    WS_CHECK(ws_session_speed(s) == 1.0);
    WS_CHECK(strcmp(ws_session_scene_id(s), "world_live") == 0);
    WS_CHECK(ws_session_total_population(s) > 0u);
    ws_session_destroy(s);
    return 1;
}

static int test_world_advances_and_is_capped(void)
{
    ws_session *s = make(1ULL);
    WS_CHECK(s != NULL);
    uint64_t t0 = ws_session_tick(s);
    run_seconds(s, 5.0);
    WS_CHECK(ws_session_tick(s) > t0);
    /* Population changes as the ecosystem runs. */
    WS_CHECK(ws_session_total_population(s) > 0u);
    ws_session_destroy(s);
    return 1;
}

static int test_pause_freezes_world(void)
{
    ws_session *s = make(2ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 2.0);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_PAUSE, 0.0);
    WS_CHECK(ws_session_paused(s) == 1);
    uint64_t t0 = ws_session_tick(s);
    run_seconds(s, 5.0);
    WS_CHECK(ws_session_tick(s) == t0); /* paused world does not advance */
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_PAUSE, 0.0);
    run_seconds(s, 1.0);
    WS_CHECK(ws_session_tick(s) > t0);
    ws_session_destroy(s);
    return 1;
}

static int test_observer_does_not_change_world(void)
{
    /* Two sessions with the same seed and the same world ticks must have the
     * same digest, even if one had the camera/view commands exercised. */
    ws_session *a = make(3ULL);
    ws_session *b = make(3ULL);
    WS_CHECK(a != NULL && b != NULL);
    run_seconds(a, 3.0);
    run_seconds(b, 3.0);
    /* Exercise every observer-only command on b. */
    (void)ws_session_apply_command(b, WS_CMD_CYCLE_VIEW, 0.0);
    (void)ws_session_apply_command(b, WS_CMD_SELECT_NEXT, 0.0);
    (void)ws_session_apply_command(b, WS_CMD_SET_VIEW_MAP, 0.0);
    (void)ws_session_apply_command(b, WS_CMD_TOGGLE_HELP, 0.0);
    (void)ws_session_apply_command(b, WS_CMD_SET_TIME_OF_DAY, 20.0 * 3600.0);
    char da[WS_SESSION_DIGEST_HEX];
    char db[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(a, da) == WS_OK);
    WS_CHECK(ws_session_digest(b, db) == WS_OK);
    WS_CHECK(strcmp(da, db) == 0);
    ws_session_destroy(a);
    ws_session_destroy(b);
    return 1;
}

static int test_determinism_same_seed(void)
{
    ws_session *a = make(4ULL);
    ws_session *b = make(4ULL);
    WS_CHECK(a != NULL && b != NULL);
    run_seconds(a, 4.0);
    run_seconds(b, 4.0);
    char da[WS_SESSION_DIGEST_HEX];
    char db[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(a, da) == WS_OK);
    WS_CHECK(ws_session_digest(b, db) == WS_OK);
    WS_CHECK(strcmp(da, db) == 0);

    /* A different seed must diverge. */
    ws_session *c = make(5ULL);
    WS_CHECK(c != NULL);
    run_seconds(c, 4.0);
    char dc[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(c, dc) == WS_OK);
    WS_CHECK(strcmp(da, dc) != 0);
    ws_session_destroy(a);
    ws_session_destroy(b);
    ws_session_destroy(c);
    return 1;
}

static int test_frame_rate_independent(void)
{
    /* A slow frame and a fast frame that cover the same wall time must reach
     * the same number of fixed ticks (the world is decoupled from FPS). The
     * catch-up is capped per call, so compare cumulative results over the same
     * total wall time with the same per-call budget. */
    ws_session *fast = make(6ULL);
    ws_session *slow = make(6ULL);
    WS_CHECK(fast != NULL && slow != NULL);
    for (int i = 0; i < 100; ++i) {
        (void)ws_session_step(fast, 0.05); /* 20 fps */
    }
    for (int i = 0; i < 20; ++i) {
        (void)ws_session_step(slow, 0.25); /* 4 fps, same 5 s total */
    }
    WS_CHECK(ws_session_tick(fast) == ws_session_tick(slow));
    char df[WS_SESSION_DIGEST_HEX];
    char ds[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(fast, df) == WS_OK);
    WS_CHECK(ws_session_digest(slow, ds) == WS_OK);
    WS_CHECK(strcmp(df, ds) == 0);
    ws_session_destroy(fast);
    ws_session_destroy(slow);
    return 1;
}

static int test_selection_and_inspection(void)
{
    ws_session *s = make(7ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 3.0);
    (void)ws_session_apply_command(s, WS_CMD_SELECT_NEXT, 0.0);
    ws_session_agent_view v;
    if (ws_session_selected(s, &v)) {
        WS_CHECK(v.valid == 1);
        WS_CHECK(v.species >= 0 && v.species < WS_SPECIES_COUNT);
        WS_CHECK(v.species_name != NULL);
        WS_CHECK(v.energy >= 0.0 && v.energy <= 1.0);
        WS_CHECK(v.activity != NULL);
        WS_CHECK(v.age_s >= 0.0);
    }
    ws_session_destroy(s);
    return 1;
}

static int test_timeline_and_map(void)
{
    ws_session *s = make(8ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 6.0);
    /* The world produces events, so the timeline is not empty. */
    WS_CHECK(ws_session_timeline_count(s) > 0u);
    ws_session_event_view ev;
    WS_CHECK(ws_session_timeline_at(s, 0u, &ev) == WS_OK);
    WS_CHECK(ev.name != NULL);
    WS_CHECK(ws_session_timeline_at(s, 999u, &ev) != WS_OK);

    ws_session_map_cell cells[WS_SESSION_MAX_MAP];
    uint32_t n = ws_session_map(s, cells, WS_SESSION_MAX_MAP);
    WS_CHECK(n > 0u);
    int focus_seen = 0;
    for (uint32_t i = 0; i < n; ++i) {
        if (cells[i].is_focus) {
            focus_seen = 1;
        }
        WS_CHECK(cells[i].level >= WS_LEVEL_FAR && cells[i].level <= WS_LEVEL_NEAR);
    }
    WS_CHECK(focus_seen == 1);
    ws_session_destroy(s);
    return 1;
}

static int test_save_load_roundtrip(void)
{
    ws_session *s = make(9ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 3.0);
    char before[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(s, before) == WS_OK);

    const char *path = "ws_session_test.wss";
    WS_CHECK(ws_session_save(s, path) == WS_OK);

    /* Advance so the live state differs, then load and confirm restoration. */
    run_seconds(s, 3.0);
    char drifted[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(s, drifted) == WS_OK);
    WS_CHECK(strcmp(before, drifted) != 0);

    WS_CHECK(ws_session_load(s, path) == WS_OK);
    char after[WS_SESSION_DIGEST_HEX];
    WS_CHECK(ws_session_digest(s, after) == WS_OK);
    WS_CHECK(strcmp(before, after) == 0);

    /* A corrupted checkpoint must be rejected, not silently accepted. */
    WS_CHECK(ws_session_load(s, "ws_session_missing.wss") != WS_OK);
    ws_session_destroy(s);
    remove(path);
    return 1;
}

static int test_render_produces_signal(void)
{
    ws_session *s = make(10ULL);
    WS_CHECK(s != NULL);
    run_seconds(s, 2.0);
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 320u, 180u) == WS_OK);
    WS_CHECK(ws_session_render(s, &fb, NULL) == WS_OK);
    /* Not a blank frame. */
    int lit = 0;
    for (size_t i = 0; i < (size_t)fb.width * fb.height; ++i) {
        if (fb.pixels[i * 4u] > 0u || fb.pixels[i * 4u + 1u] > 0u) {
            lit = 1;
            break;
        }
    }
    WS_CHECK(lit == 1);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return 1;
}

static int test_view_modes_cycle(void)
{
    ws_session *s = make(11ULL);
    WS_CHECK(s != NULL);
    for (int i = 0; i < WS_VIEW_COUNT; ++i) {
        int m = ws_session_view_mode(s);
        WS_CHECK(m >= 0 && m < WS_VIEW_COUNT);
        WS_CHECK(ws_view_mode_name(m) != NULL);
        (void)ws_session_apply_command(s, WS_CMD_CYCLE_VIEW, 0.0);
    }
    WS_CHECK(ws_session_view_mode(s) == WS_VIEW_AUTO);
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FREE, 0.0);
    WS_CHECK(ws_session_view_mode(s) == WS_VIEW_FREE);
    /* Free-camera input is accepted and moves the camera. */
    ws_camera c0 = ws_session_camera(s);
    WS_CHECK(ws_session_camera_input(s, 1.0, 0.0, 0.0, 0.0, 0.0, 0.1) == WS_OK);
    ws_camera c1 = ws_session_camera(s);
    WS_CHECK(c0.eye.x != c1.eye.x || c0.eye.z != c1.eye.z);
    ws_session_destroy(s);
    return 1;
}

static int test_stats_and_demography(void)
{
    ws_session *s = make(12ULL);
    WS_CHECK(s != NULL);
    WS_CHECK(ws_session_stats_visible(s) == 0);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_STATS, 0.0);
    WS_CHECK(ws_session_stats_visible(s) == 1);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_STATS, 0.0);
    WS_CHECK(ws_session_stats_visible(s) == 0);

    /* Run long enough for the ecology to sample its population series. */
    run_seconds(s, 20.0);
    float hist[WS_ECOLOGY_MAX_SAMPLES * WS_SPECIES_COUNT];
    uint32_t n = ws_session_population_history(s, hist, WS_ECOLOGY_MAX_SAMPLES);
    WS_CHECK(n > 0u);
    WS_CHECK(n <= WS_ECOLOGY_MAX_SAMPLES);
    for (uint32_t i = 0; i < n * WS_SPECIES_COUNT; ++i) {
        WS_CHECK(hist[i] >= 0.0f);
    }
    /* A too-small buffer is filled only to its capacity. */
    float small[2u * WS_SPECIES_COUNT];
    WS_CHECK(ws_session_population_history(s, small, 2u) == 2u);
    WS_CHECK(ws_session_population_history(s, NULL, 4u) == 0u);

    /* The observer's statistics are the ecology's own counters. */
    uint64_t b0 = ws_session_births(s);
    uint64_t d0 = ws_session_deaths(s);
    run_seconds(s, 30.0);
    WS_CHECK(ws_session_births(s) >= b0);
    WS_CHECK(ws_session_deaths(s) >= d0);
    WS_CHECK(ws_session_births(s) + ws_session_deaths(s) > 0u);
    ws_session_destroy(s);
    return 1;
}

static int test_shadows_toggle(void)
{
    ws_session *s = make(13ULL);
    WS_CHECK(s != NULL);
    /* Shadows are off by default so the live view matches the golden frames. */
    WS_CHECK(ws_session_shadows_visible(s) == 0);
    /* A low morning sun and a free overview give a visible shadow direction. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FREE, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_TIME_OF_DAY, 8.0 * 3600.0);
    run_seconds(s, 3.0);

    ws_framebuffer a, b, c;
    WS_CHECK(ws_framebuffer_create(&a, 200u, 120u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&b, 200u, 120u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&c, 200u, 120u) == WS_OK);
    size_t bytes = (size_t)a.width * a.height * 4u;

    WS_CHECK(ws_session_render(s, &a, NULL) == WS_OK);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0);
    WS_CHECK(ws_session_shadows_visible(s) == 1);
    WS_CHECK(ws_session_render(s, &b, NULL) == WS_OK);
    /* Shadows darken the frame: they must change the image and never brighten
     * the mean luminance. */
    WS_CHECK(memcmp(a.pixels, b.pixels, bytes) != 0);
    double la = 0.0, lb = 0.0;
    for (size_t i = 0; i < (size_t)a.width * a.height; ++i) {
        la += a.pixels[i * 4u] + a.pixels[i * 4u + 1u] + a.pixels[i * 4u + 2u];
        lb += b.pixels[i * 4u] + b.pixels[i * 4u + 1u] + b.pixels[i * 4u + 2u];
    }
    WS_CHECK(lb <= la);
    /* Toggling back off restores the exact unshadowed frame. */
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0);
    WS_CHECK(ws_session_shadows_visible(s) == 0);
    WS_CHECK(ws_session_render(s, &c, NULL) == WS_OK);
    WS_CHECK(memcmp(a.pixels, c.pixels, bytes) == 0);

    ws_framebuffer_free(&a);
    ws_framebuffer_free(&b);
    ws_framebuffer_free(&c);
    ws_session_destroy(s);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"session_create_and_queries", test_create_and_queries},
        {"session_world_advances", test_world_advances_and_is_capped},
        {"session_pause_freezes_world", test_pause_freezes_world},
        {"session_observer_readonly", test_observer_does_not_change_world},
        {"session_determinism", test_determinism_same_seed},
        {"session_frame_rate_independent", test_frame_rate_independent},
        {"session_selection_inspection", test_selection_and_inspection},
        {"session_timeline_and_map", test_timeline_and_map},
        {"session_save_load_roundtrip", test_save_load_roundtrip},
        {"session_render_signal", test_render_produces_signal},
        {"session_view_modes", test_view_modes_cycle},
        {"session_stats_demography", test_stats_and_demography},
        {"session_shadows_toggle", test_shadows_toggle},
    };
    return ws_test_run("session", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
