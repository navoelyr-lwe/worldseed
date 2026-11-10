/* Worldseed Engine — interactive live application (P11). */
#include "ws_live.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_hash.h"
#include "../platform/ws_clock.h"
#include "../platform/ws_fs.h"
#include "../render/ws_image.h"
#include "../ui/ws_hud.h"
#include "../ui/ws_session.h"

ws_live_options ws_live_options_default(void)
{
    ws_live_options o;
    o.enabled = 0;
    o.headless = 0;
    o.fullscreen = 0;
    o.seconds = 0.0;
    o.tour = 0;
    o.replay = 0;
    o.out_dir = NULL;
    o.capture_path = NULL;
    o.seed = 20260101ULL;
    o.width = 1280u;
    o.height = 720u;
    o.start_view = WS_VIEW_AUTO;
    o.start_time_of_day_s = 10.0 * 3600.0;
    o.render_scale = 0; /* auto */
    o.settings_path = "worldseed.cfg";
    o.set_flags = WS_LIVE_SET_NONE;
    return o;
}

static void ws_live_merge_settings(ws_live_options *opts);

ws_result ws_live_parse_options(int argc, const char *const *argv, ws_live_options *opts)
{
    if (opts == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *opts = ws_live_options_default();
    for (int i = 1; i < argc; ++i) {
        const char *a = argv[i];
        if (strcmp(a, "--live") == 0) {
            opts->enabled = 1;
        } else if (strcmp(a, "--fullscreen") == 0) {
            opts->enabled = 1;
            opts->fullscreen = 1;
            opts->set_flags |= WS_LIVE_SET_FULLSCREEN;
        } else if (strcmp(a, "--live-settings") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->settings_path = argv[++i];
        } else if (strcmp(a, "--live-no-settings") == 0) {
            /* Ignore any saved settings file and do not write one (used by the
             * evidence runs so they stay independent of the developer's config). */
            opts->settings_path = NULL;
        } else if (strcmp(a, "--live-headless") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->headless = 1;
            opts->seconds = strtod(argv[++i], NULL);
            if (!(opts->seconds > 0.0)) {
                return WS_ERR_INVALID_ARGUMENT;
            }
        } else if (strcmp(a, "--live-tour") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->headless = 1;
            opts->tour = 1;
            opts->out_dir = argv[++i];
        } else if (strcmp(a, "--live-replay") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->headless = 1;
            opts->replay = 1;
            opts->out_dir = argv[++i];
        } else if (strcmp(a, "--live-capture") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->headless = 1;
            opts->capture_path = argv[++i];
        } else if (strcmp(a, "--live-seed") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->seed = strtoull(argv[++i], NULL, 10);
            opts->set_flags |= WS_LIVE_SET_SEED;
        } else if (strcmp(a, "--live-view") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            long v = strtol(argv[++i], NULL, 10);
            if (v < 0 || v >= WS_VIEW_COUNT) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->start_view = (int)v;
            opts->set_flags |= WS_LIVE_SET_VIEW;
        } else if (strcmp(a, "--live-time") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->start_time_of_day_s = strtod(argv[++i], NULL);
            opts->set_flags |= WS_LIVE_SET_TIME;
        } else if (strcmp(a, "--size") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            unsigned w = 0u;
            unsigned h = 0u;
            if (sscanf(argv[++i], "%ux%u", &w, &h) != 2 || w == 0u || h == 0u ||
                w > 16384u || h > 16384u) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->width = w;
            opts->height = h;
            opts->set_flags |= WS_LIVE_SET_SIZE;
        } else if (strcmp(a, "--live-render-scale") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            long v = strtol(argv[++i], NULL, 10);
            if (v < 1 || v > 4) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->render_scale = (int)v;
            opts->set_flags |= WS_LIVE_SET_RENDER_SCALE;
        }
        /* Other non-live flags are handled by the showcase parser; the live run
         * only needs the resolution, which it takes from --size above. */
    }
    /* Resolve the effective configuration once, here, so both the windowed and
     * headless paths (and the tests) see the same merged options. */
    if (opts->enabled) {
        ws_live_merge_settings(opts);
    }
    return WS_OK;
}

/* Fold the saved settings file into the options. An explicit command-line flag
 * always wins (its WS_LIVE_SET_* bit is set); everything else comes from the
 * file, and fields with no flag and no file value keep the built-in default. */
static void ws_live_merge_settings(ws_live_options *opts)
{
    ws_settings saved = ws_settings_default();
    /* Headless evidence runs are hermetic: they never read or write the user's
     * settings file, so CI results do not depend on a developer's config. */
    int use_file = (opts->settings_path != NULL) && !opts->headless;
    if (use_file && ws_settings_load(opts->settings_path, &saved) != WS_OK) {
        saved = ws_settings_default();
    }
    if ((opts->set_flags & WS_LIVE_SET_SEED) != 0u) {
        saved.seed = opts->seed;
    }
    if ((opts->set_flags & WS_LIVE_SET_SIZE) != 0u) {
        saved.width = opts->width;
        saved.height = opts->height;
    }
    if ((opts->set_flags & WS_LIVE_SET_VIEW) != 0u) {
        saved.view = opts->start_view;
    }
    if ((opts->set_flags & WS_LIVE_SET_TIME) != 0u) {
        saved.time_of_day_s = opts->start_time_of_day_s;
    }
    if ((opts->set_flags & WS_LIVE_SET_RENDER_SCALE) != 0u) {
        saved.render_scale = opts->render_scale;
    }
    if ((opts->set_flags & WS_LIVE_SET_FULLSCREEN) != 0u) {
        saved.fullscreen = opts->fullscreen;
    }

    /* Write the effective values back so the rest of the app uses one source. */
    opts->seed = saved.seed;
    opts->width = saved.width;
    opts->height = saved.height;
    opts->start_view = saved.view;
    opts->start_time_of_day_s = saved.time_of_day_s;
    opts->render_scale = saved.render_scale;
    opts->fullscreen = saved.fullscreen;
    opts->settings = saved;
}

/* Snapshot the observer's settings plus the window fields the session does not
 * own, and write them to the settings file. Called on a clean exit so the user's
 * choices persist; failures are non-fatal (a read-only location just means no
 * persistence). */
void ws_live_save_settings(const ws_live_options *opts, const ws_session *s)
{
    if (opts == NULL || opts->settings_path == NULL || s == NULL) {
        return;
    }
    ws_settings out;
    if (ws_session_get_settings(s, &out) != WS_OK) {
        return;
    }
    out.width = opts->width;
    out.height = opts->height;
    out.render_scale = opts->render_scale;
    out.fullscreen = opts->fullscreen;
    (void)ws_settings_save(opts->settings_path, &out);
}

ws_result ws_live_make_session(const ws_live_options *opts, ws_session **out)
{
    if (opts == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out = NULL;
    ws_session_config cfg = ws_session_config_default(opts->seed);
    cfg.viewport_w = opts->width;
    cfg.viewport_h = opts->height;
    cfg.start_time_of_day_s = opts->start_time_of_day_s;
    ws_session *s = NULL;
    ws_result r = ws_session_create(&s, &cfg);
    if (r != WS_OK) {
        return r;
    }
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_AUTO + opts->start_view, 0.0);
    (void)ws_session_apply_settings(s, &opts->settings);
    *out = s;
    return WS_OK;
}

/* --- headless evidence run ---------------------------------------------- */

static int run_headless(const ws_live_options *opts)
{
    ws_session *s = NULL;
    if (ws_live_make_session(opts, &s) != WS_OK) {
        fprintf(stderr, "live: session create failed\n");
        return 1;
    }
    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, opts->width, opts->height) != WS_OK) {
        fprintf(stderr, "live: framebuffer allocation failed\n");
        ws_session_destroy(s);
        return 1;
    }
    float *depth = malloc((size_t)opts->width * opts->height * sizeof(float));
    if (depth == NULL) {
        fprintf(stderr, "live: depth allocation failed\n");
        ws_framebuffer_free(&fb);
        ws_session_destroy(s);
        return 1;
    }

    const double dt = 1.0 / 60.0;
    int steps = (int)(opts->seconds / dt);
    uint64_t frames = 0u;
    double worst_ms = 0.0;
    double total_ms = 0.0;
    double total_step_ms = 0.0;
    double total_render_ms = 0.0;
    double total_hud_ms = 0.0;
    for (int i = 0; i < steps; ++i) {
        uint64_t t0 = ws_clock_monotonic_ns();
        if (ws_session_step(s, dt) != WS_OK) {
            fprintf(stderr, "live: step failed\n");
            break;
        }
        uint64_t t1 = ws_clock_monotonic_ns();
        if (ws_session_render(s, &fb, depth) != WS_OK) {
            fprintf(stderr, "live: render failed\n");
            break;
        }
        uint64_t t2 = ws_clock_monotonic_ns();
        ws_hud_options ho = ws_hud_options_default();
        ws_hud_draw(&fb, s, &ho);
        uint64_t t3 = ws_clock_monotonic_ns();
        double ms = ws_clock_ns_to_seconds(t3 - t0) * 1000.0;
        total_step_ms += ws_clock_ns_to_seconds(t1 - t0) * 1000.0;
        total_render_ms += ws_clock_ns_to_seconds(t2 - t1) * 1000.0;
        total_hud_ms += ws_clock_ns_to_seconds(t3 - t2) * 1000.0;
        ws_session_note_frame(s, ms);
        if (ms > worst_ms) { worst_ms = ms; }
        total_ms += ms;
        frames += 1u;
    }

    double coverage = 0.0;
    uint8_t bg[3] = {24u, 28u, 36u};
    (void)ws_image_coverage(&fb, bg, 8, &coverage);
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    ws_sha256_buffer(fb.pixels, (size_t)fb.width * fb.height * 4u, digest);
    ws_sha256_hex(digest, hex);
    char wdigest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, wdigest);

    printf("LIVE_HEADLESS_OK frames=%llu sim_seconds=%.1f world_tick=%llu "
           "avg_ms=%.2f step_ms=%.2f render_ms=%.2f hud_ms=%.2f worst_ms=%.2f fps=%.1f "
           "coverage=%.4f frame_sha256=%s world_digest=%s pop=%u loaded=%u\n",
           (unsigned long long)frames, opts->seconds,
           (unsigned long long)ws_session_tick(s),
           frames ? total_ms / (double)frames : 0.0,
           frames ? total_step_ms / (double)frames : 0.0,
           frames ? total_render_ms / (double)frames : 0.0,
           frames ? total_hud_ms / (double)frames : 0.0, worst_ms,
           frames ? 1000.0 / (total_ms / (double)frames) : 0.0, coverage, hex,
           wdigest, ws_session_total_population(s), ws_session_loaded_regions(s));

    if (opts->capture_path != NULL) {
        if (ws_framebuffer_write_ppm(&fb, opts->capture_path) != WS_OK) {
            fprintf(stderr, "live: cannot write %s\n", opts->capture_path);
        } else {
            printf("live capture path=%s\n", opts->capture_path);
        }
    }

    free(depth);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    return frames > 0u ? 0 : 1;
}

/* --- scripted evidence tour --------------------------------------------- */

static const char *TOUR_STEPS[] = {
    "auto_day", "free_overview", "follow_deer", "map_overview",
    "auto_dusk", "follow_fox", "auto_night", "reload_check",
    "shadows_off", "shadows_on",
};

static int capture_step(ws_session *s, ws_framebuffer *fb, float *depth,
                        const ws_live_options *opts, const char *name,
                        int index)
{
    if (ws_session_render(s, fb, depth) != WS_OK) {
        return 0;
    }
    ws_hud_options ho = ws_hud_options_default();
    ws_hud_draw(fb, s, &ho);
    char path[512];
    snprintf(path, sizeof(path), "%s/step-%02d-%s.ppm", opts->out_dir, index, name);
    if (ws_framebuffer_write_ppm(fb, path) != WS_OK) {
        fprintf(stderr, "live tour: cannot write %s\n", path);
        return 0;
    }
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    ws_sha256_buffer(fb->pixels, (size_t)fb->width * fb->height * 4u, digest);
    ws_sha256_hex(digest, hex);
    double coverage = 0.0;
    uint8_t bg[3] = {24u, 28u, 36u};
    (void)ws_image_coverage(fb, bg, 8, &coverage);
    printf("tour step=%02d name=%s path=%s sha256=%s coverage=%.4f\n",
           index, name, path, hex, coverage);
    return 1;
}

static int run_tour(const ws_live_options *opts)
{
    if (ws_fs_make_dirs(opts->out_dir) != WS_OK) {
        fprintf(stderr, "live tour: cannot create %s\n", opts->out_dir);
        return 1;
    }
    ws_session *s = NULL;
    if (ws_live_make_session(opts, &s) != WS_OK) {
        fprintf(stderr, "live tour: session create failed\n");
        return 1;
    }
    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, opts->width, opts->height) != WS_OK) {
        ws_session_destroy(s);
        return 1;
    }
    float *depth = malloc((size_t)opts->width * opts->height * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        ws_session_destroy(s);
        return 1;
    }

    char manifest[512];
    snprintf(manifest, sizeof(manifest), "%s/manifest.txt", opts->out_dir);
    FILE *mf = fopen(manifest, "w");

    int ok = 1;
    int step = 0;
    /* 1: auto day. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_AUTO, 0.0);
    for (int i = 0; i < 200; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 2: free overview. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FREE, 0.0);
    for (int i = 0; i < 40; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 3: follow the selected animal. */
    (void)ws_session_apply_command(s, WS_CMD_SELECT_NEXT, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FOLLOW, 0.0);
    for (int i = 0; i < 60; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 4: map overview. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_MAP, 0.0);
    for (int i = 0; i < 20; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 5: dusk, auto. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_AUTO, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_TIME_OF_DAY, 19.0 * 3600.0);
    for (int i = 0; i < 80; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 6: follow a second animal. */
    (void)ws_session_apply_command(s, WS_CMD_SELECT_NEXT, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FOLLOW, 0.0);
    for (int i = 0; i < 60; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 7: night, auto, with the statistics panel on. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_AUTO, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_TIME_OF_DAY, 23.0 * 3600.0);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_STATS, 0.0);
    for (int i = 0; i < 80; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 8: save, advance, load, and confirm the digest is restored. */
    char save_path[512];
    snprintf(save_path, sizeof(save_path), "%s/checkpoint.wss", opts->out_dir);
    char before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, before);
    int saved = ws_session_save(s, save_path) == WS_OK;
    for (int i = 0; i < 100; ++i) { (void)ws_session_step(s, 0.05); }
    int loaded = ws_session_load(s, save_path) == WS_OK;
    char after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, after);
    int roundtrip = saved && loaded && strcmp(before, after) == 0;
    printf("tour save_load saved=%d loaded=%d roundtrip=%d\n", saved, loaded, roundtrip);
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    ok &= roundtrip;

    /* 9-10: shadows off vs on, same low morning sun and same free overview, so
     * the pair isolates the directional-shadow effect (docs/08, P12). */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FREE, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_SET_TIME_OF_DAY, 8.0 * 3600.0);
    for (int i = 0; i < 40; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0);
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0); /* back off */

    /* Prove the directional shadows really change the live frame and only ever
     * darken it (they attenuate the sun term, never the ambient). Same world,
     * same camera: only the shadow switch differs between the two renders. */
    int shadow_pair_changed = 0;
    {
        size_t bytes = (size_t)fb.width * fb.height * 4u;
        uint8_t *off = malloc(bytes);
        if (off != NULL) {
            ok &= ws_session_render(s, &fb, depth) == WS_OK;
            memcpy(off, fb.pixels, bytes);
            (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0);
            ok &= ws_session_render(s, &fb, depth) == WS_OK;
            (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SHADOWS, 0.0);
            size_t on_sum = 0u, off_sum = 0u;
            for (size_t i = 0; i < (size_t)fb.width * fb.height; ++i) {
                off_sum += off[i * 4u] + off[i * 4u + 1u] + off[i * 4u + 2u];
                on_sum += fb.pixels[i * 4u] + fb.pixels[i * 4u + 1u] + fb.pixels[i * 4u + 2u];
            }
            shadow_pair_changed = (memcmp(off, fb.pixels, bytes) != 0) && (on_sum <= off_sum);
            free(off);
        }
    }
    ok &= shadow_pair_changed;

    if (mf != NULL) {
        fprintf(mf, "# Worldseed Engine P11 live tour manifest\n");
        fprintf(mf, "seed=%llu\n", (unsigned long long)opts->seed);
        fprintf(mf, "size=%ux%u\n", opts->width, opts->height);
        fprintf(mf, "world_tick=%llu\n", (unsigned long long)ws_session_tick(s));
        fprintf(mf, "population=%u\n", ws_session_total_population(s));
        fprintf(mf, "loaded_regions=%u\n", ws_session_loaded_regions(s));
        fprintf(mf, "save_load_roundtrip=%d\n", roundtrip);
        fprintf(mf, "shadows_verified=%d\n", shadow_pair_changed);
        fprintf(mf, "steps=%d\n", step);
        fclose(mf);
    }

    free(depth);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    if (ok) {
        printf("LIVE_TOUR_OK steps=%d dir=%s\n", step, opts->out_dir);
    }
    return ok ? 0 : 1;
}

/* --- replay evidence (docs/17 item 8) ----------------------------------- */

static int run_replay(const ws_live_options *opts)
{
    if (ws_fs_make_dirs(opts->out_dir) != WS_OK) {
        fprintf(stderr, "live replay: cannot create %s\n", opts->out_dir);
        return 1;
    }
    ws_session *s = NULL;
    if (ws_live_make_session(opts, &s) != WS_OK) {
        fprintf(stderr, "live replay: session create failed\n");
        return 1;
    }
    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, opts->width, opts->height) != WS_OK) {
        ws_session_destroy(s);
        return 1;
    }
    float *depth = malloc((size_t)opts->width * opts->height * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        ws_session_destroy(s);
        return 1;
    }

    int ok = 1;
    int step = 0;
    /* Let the world run so the replay ring holds real history. */
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_AUTO, 0.0);
    for (int i = 0; i < 240; ++i) { (void)ws_session_step(s, 0.05); }

    char live_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, live_digest);

    uint32_t samples = ws_session_replay_samples(s);
    double span = ws_session_replay_span_s(s);
    ok &= samples >= 2u;
    ok &= span > 0.0;

    /* Enter replay at the oldest retained sample. */
    (void)ws_session_apply_command(s, WS_CMD_REPLAY_TOGGLE, 0.0);
    int active = ws_session_replay_active(s);
    ok &= active == 1;
    ok &= capture_step(s, &fb, depth, opts, "replay_oldest", step); step++;

    char oldest_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, oldest_digest);
    ok &= strcmp(oldest_digest, live_digest) != 0;

    /* Step forward through the history. */
    for (int i = 0; i < 4; ++i) {
        (void)ws_session_apply_command(s, WS_CMD_REPLAY_STEP_FORWARD, 0.0);
    }
    ok &= capture_step(s, &fb, depth, opts, "replay_stepped", step); step++;

    /* Seek to the newest sample: the reconstructed world matches the live one. */
    (void)ws_session_apply_command(s, WS_CMD_REPLAY_SEEK, 1.0);
    char newest_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, newest_digest);
    int newest_matches_live = strcmp(newest_digest, live_digest) == 0;
    ok &= newest_matches_live;
    ok &= capture_step(s, &fb, depth, opts, "replay_newest", step); step++;

    /* Leave replay: the live world is restored bit-exactly and resumes. */
    (void)ws_session_apply_command(s, WS_CMD_REPLAY_TOGGLE, 0.0);
    ok &= ws_session_replay_active(s) == 0;
    char after_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, after_digest);
    int restored = strcmp(after_digest, live_digest) == 0;
    ok &= restored;
    uint64_t step_before = ws_session_steps(s);
    for (int i = 0; i < 40; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= ws_session_steps(s) > step_before;

    char manifest[512];
    snprintf(manifest, sizeof(manifest), "%s/replay-manifest.txt", opts->out_dir);
    FILE *mf = fopen(manifest, "w");
    if (mf != NULL) {
        fprintf(mf, "# Worldseed Engine P11 replay manifest\n");
        fprintf(mf, "seed=%llu\n", (unsigned long long)opts->seed);
        fprintf(mf, "size=%ux%u\n", opts->width, opts->height);
        fprintf(mf, "samples=%u\n", samples);
        fprintf(mf, "span_seconds=%.1f\n", span);
        fprintf(mf, "newest_matches_live=%d\n", newest_matches_live);
        fprintf(mf, "live_restored=%d\n", restored);
        fprintf(mf, "live_digest=%s\n", live_digest);
        fprintf(mf, "newest_digest=%s\n", newest_digest);
        fprintf(mf, "steps=%d\n", step);
        fclose(mf);
    }

    free(depth);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    if (ok) {
        printf("LIVE_REPLAY_OK samples=%u span=%.1f newest_matches_live=%d "
               "live_restored=%d steps=%d dir=%s\n",
               samples, span, newest_matches_live, restored, step, opts->out_dir);
    }
    return ok ? 0 : 1;
}

int ws_live_run(const ws_live_options *opts)
{
    if (opts == NULL || !opts->enabled) {
        return 2;
    }
    /* Options arrive already merged (parse_options resolves the settings file),
     * so the windowed and headless paths drive exactly the same product. */
    if (opts->tour) {
        return run_tour(opts);
    }
    if (opts->replay) {
        return run_replay(opts);
    }
    if (opts->headless) {
        return run_headless(opts);
    }
    return ws_live_present(opts);
}
