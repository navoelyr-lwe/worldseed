/* Worldseed Engine — interactive live application (P11). */
#include "ws_live.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_hash.h"
#include "../audio/ws_audio.h"
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
    o.worldselect = 0;
    o.export_mode = 0;
    o.export_seconds = 0.0;
    o.no_select = 0;
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
        } else if (strcmp(a, "--live-gpu") == 0) {
            /* Opt in to the OpenCL full-scene render path (CPU fallback). Off by
             * default: the CPU renderer stays the golden/determinism reference. */
            opts->enabled = 1;
            opts->set_flags |= WS_LIVE_SET_GPU;
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
        } else if (strcmp(a, "--live-select") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->headless = 1;
            opts->worldselect = 1;
            opts->out_dir = argv[++i];
        } else if (strcmp(a, "--live-export") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->headless = 1;
            opts->export_mode = 1;
            opts->out_dir = argv[++i];
            opts->export_seconds = 2.0;
        } else if (strcmp(a, "--live-export-seconds") == 0) {
            if (i + 1 >= argc) {
                return WS_ERR_INVALID_ARGUMENT;
            }
            opts->enabled = 1;
            opts->export_seconds = strtod(argv[++i], NULL);
            if (!(opts->export_seconds > 0.0)) {
                return WS_ERR_INVALID_ARGUMENT;
            }
        } else if (strcmp(a, "--live-no-select") == 0) {
            /* Skip the startup world-selection screen and open the world
             * directly (used by the GUI/evidence runs so they present frames). */
            opts->enabled = 1;
            opts->no_select = 1;
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
    if ((opts->set_flags & WS_LIVE_SET_GPU) != 0u) {
        saved.gpu_render = 1;
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
    /* The session owns the observer fields including any resolution/scale/text
     * size the user changed from the in-window settings menu; only fullscreen is
     * a pure window operation that the session does not track. */
    out.fullscreen = opts->fullscreen;
    (void)ws_settings_save(opts->settings_path, &out);
}

/* Seed the "create a new world" choice derives a fresh world from. Mixing the
 * configured seed with the wall clock gives a different world each session while
 * staying a plain deterministic 64-bit value the session can rebuild from. */
uint64_t ws_live_new_world_seed(uint64_t base)
{
    uint64_t t = (uint64_t)(ws_clock_monotonic_ns() / 1000000u);
    uint64_t z = base + 0x9E3779B97F4A7C15ULL + t;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z = z ^ (z >> 31);
    return z != 0u ? z : base;
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
    /* Give the in-window settings menu a file to save to/reload from. */
    (void)ws_session_set_settings_path(s, opts->settings_path);
    *out = s;
    return WS_OK;
}

void ws_live_startup_world_select(const ws_live_options *opts, ws_session *s)
{
    if (opts == NULL || s == NULL || opts->no_select) {
        return;
    }
    /* "Create a new world" should differ each session, so derive its seed from
     * the wall clock. The session rebuilds deterministically from whatever seed
     * it is handed (the sample and load choices stay fixed). */
    ws_session_world_set_new_seed(s, ws_live_new_world_seed(opts->seed));
    /* Default the cursor to "load" when a checkpoint exists, else "new". */
    int start = ws_session_world_select_can_load(s) ? WS_WORLD_LOAD : WS_WORLD_NEW;
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0);
    while (ws_session_world_select_cursor(s) < start) {
        (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, 1.0);
    }
    while (ws_session_world_select_cursor(s) > start) {
        (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, -1.0);
    }
}

/* --- headless evidence run ---------------------------------------------- */

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) { return -1; }
    if (da > db) { return 1; }
    return 0;
}

double ws_live_percentile_ms(double *samples, size_t count, double p)
{
    if (samples == NULL || count == 0u) {
        return 0.0;
    }
    if (p < 0.0) { p = 0.0; }
    if (p > 1.0) { p = 1.0; }
    qsort(samples, count, sizeof(double), cmp_double);
    size_t idx = (size_t)(p * (double)(count - 1u));
    if (idx >= count) { idx = count - 1u; }
    return samples[idx];
}

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
    /* Frame-time samples for the documented percentile gate (docs/11: the
     * release metric is p95, which a single FPS sample cannot stand in for). */
    size_t cap = 4096u;
    double *samples = (double *)malloc(cap * sizeof(double));
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
        if (samples != NULL) {
            if (frames == cap) {
                size_t ncap = cap * 2u;
                double *grown = (double *)realloc(samples, ncap * sizeof(double));
                if (grown != NULL) { samples = grown; cap = ncap; }
            }
            if (frames < cap) { samples[frames] = ms; }
        }
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

    double p50 = 0.0, p95 = 0.0, p99 = 0.0;
    if (samples != NULL && frames > 0u) {
        p50 = ws_live_percentile_ms(samples, (size_t)frames, 0.50);
        p95 = ws_live_percentile_ms(samples, (size_t)frames, 0.95);
        p99 = ws_live_percentile_ms(samples, (size_t)frames, 0.99);
    }

    printf("LIVE_HEADLESS_OK frames=%llu sim_seconds=%.1f world_tick=%llu "
           "avg_ms=%.2f step_ms=%.2f render_ms=%.2f hud_ms=%.2f worst_ms=%.2f fps=%.1f "
           "p50_ms=%.2f p95_ms=%.2f p99_ms=%.2f "
           "coverage=%.4f frame_sha256=%s world_digest=%s pop=%u loaded=%u\n",
           (unsigned long long)frames, opts->seconds,
           (unsigned long long)ws_session_tick(s),
           frames ? total_ms / (double)frames : 0.0,
           frames ? total_step_ms / (double)frames : 0.0,
           frames ? total_render_ms / (double)frames : 0.0,
           frames ? total_hud_ms / (double)frames : 0.0, worst_ms,
           frames ? 1000.0 / (total_ms / (double)frames) : 0.0, p50, p95, p99, coverage, hex,
           wdigest, ws_session_total_population(s), ws_session_loaded_regions(s));
    free(samples);

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
    "shadows_off", "shadows_on", "single_step", "settings_menu",
    "world_select", "menu_click", "camera_collision", "tree_avoidance",
};

static int capture_step(ws_session *s, ws_framebuffer *fb, float *depth,
                        const ws_live_options *opts, const char *name,
                        int index)
{
    if (ws_session_render(s, fb, depth) != WS_OK) {
        return 0;
    }
    ws_hud_options ho = ws_hud_options_default();
    /* The tour frames honour the live "Text size" setting so the settings-menu
     * step visibly changes at large text. */
    ho.scale = ws_session_hud_scale(s);
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

    /* 11: single-step time control (docs/17). While paused, each step advances
     * exactly one fixed world tick and never more; the step is deterministic. */
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_PAUSE, 0.0);
    uint64_t paused_tick = ws_session_tick(s);
    uint64_t single_step_tick = paused_tick;
    for (int i = 0; i < 5; ++i) {
        (void)ws_session_apply_command(s, WS_CMD_STEP_ONCE, 0.0);
    }
    single_step_tick = ws_session_tick(s);
    int single_step_ok = ws_session_paused(s) && single_step_tick == paused_tick + 5u;
    printf("tour single_step paused=%d from=%llu to=%llu ok=%d\n",
           ws_session_paused(s), (unsigned long long)paused_tick,
           (unsigned long long)single_step_tick, single_step_ok);
    ok &= single_step_ok;
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;

    /* 12: interactive settings menu (docs/17 item 9). Open the menu, move the
     * cursor down and right, and confirm the change is real: the "Text size"
     * row goes small -> large and the world digest is untouched (the menu is
     * observer-only). The world is still paused from the single-step step. */
    char menu_before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, menu_before);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SETTINGS, 0.0);
    int menu_open = ws_session_settings_visible(s);
    int text_before = ws_session_hud_scale(s);
    /* Walk the cursor to the "Text size" row (row WS_SET_ITEM_HUD_SCALE). */
    while (ws_session_settings_cursor(s) < WS_SET_ITEM_HUD_SCALE) {
        (void)ws_session_apply_command(s, WS_CMD_SETTINGS_MOVE, 1.0);
    }
    while (ws_session_settings_cursor(s) > WS_SET_ITEM_HUD_SCALE) {
        (void)ws_session_apply_command(s, WS_CMD_SETTINGS_MOVE, -1.0);
    }
    (void)ws_session_apply_command(s, WS_CMD_SETTINGS_ADJUST, 1.0); /* small->large */
    int text_after = ws_session_hud_scale(s);
    /* Also exercise the "Master volume" row (docs/17 item 9 audio setting). */
    int vol_before = ws_session_volume(s);
    while (ws_session_settings_cursor(s) < WS_SET_ITEM_VOLUME) {
        (void)ws_session_apply_command(s, WS_CMD_SETTINGS_MOVE, 1.0);
    }
    (void)ws_session_apply_command(s, WS_CMD_SETTINGS_ADJUST, -1.0);
    int vol_after = ws_session_volume(s);
    /* And the "Camera speed" row (docs/17 item 9 input/camera setting). */
    int cam_before = ws_session_camera_speed_pct(s);
    while (ws_session_settings_cursor(s) < WS_SET_ITEM_CAM_SPEED) {
        (void)ws_session_apply_command(s, WS_CMD_SETTINGS_MOVE, 1.0);
    }
    (void)ws_session_apply_command(s, WS_CMD_SETTINGS_ADJUST, 1.0);
    int cam_after = ws_session_camera_speed_pct(s);
    char menu_after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, menu_after);
    int menu_changed = (text_before != text_after) && (text_after == 2) &&
                       (vol_after == vol_before - 10) &&
                       (cam_after == cam_before + 25);
    int menu_readonly = strcmp(menu_before, menu_after) == 0;
    printf("tour settings_menu open=%d text %d->%d volume %d->%d cam %d->%d changed=%d readonly=%d\n",
           menu_open, text_before, text_after, vol_before, vol_after,
           cam_before, cam_after, menu_changed, menu_readonly);
    ok &= menu_open && menu_changed && menu_readonly;
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SETTINGS, 0.0); /* close menu */

    /* Step 13: the world-selection screen (docs/17 "Ana deneyim"): new / load /
     * sample. The tour only opens and closes it (observer-only); the actual
     * create/load/sample behaviour is proven by --live-select. */
    char select_before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, select_before);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0);
    int select_open = ws_session_world_select_visible(s);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, 1.0);
    int select_cursor = ws_session_world_select_cursor(s);
    ok &= select_open == 1 && select_cursor >= 0;
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0); /* close */
    char select_after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, select_after);
    int select_readonly = strcmp(select_before, select_after) == 0;
    printf("tour world_select open=%d cursor=%d readonly=%d\n",
           select_open, select_cursor, select_readonly);
    ok &= select_readonly;

    /* Step 14: mouse-driven menu selection (docs/17 item 9 / "Ana deneyim").
     * A left click must land on the row the user sees. We locate a row with the
     * HUD's own hit-test (the geometry it draws), select it, then click it again
     * to activate it, and confirm the action happened while the world digest is
     * untouched. The world-selection screen accepts clicks the same way. */
    char click_before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, click_before);
    int click_scale = ws_session_hud_scale(s);
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SETTINGS, 0.0); /* open */
    /* Park the cursor on a different row so the click visibly moves it. */
    while (ws_session_settings_cursor(s) != WS_SET_ITEM_SPEED) {
        double d = ws_session_settings_cursor(s) > WS_SET_ITEM_SPEED ? -1.0 : 1.0;
        (void)ws_session_apply_command(s, WS_CMD_SETTINGS_MOVE, d);
    }
    int width_before = ws_session_settings_item_value(s, WS_SET_ITEM_WIDTH);
    int s_py = -1;
    for (int y = 0; y < (int)opts->height && s_py < 0; ++y) {
        if (ws_hud_settings_row_at(opts->width, opts->height, click_scale, 24, y)
            == WS_SET_ITEM_WIDTH) {
            s_py = y;
        }
    }
    int click_row_ok = s_py >= 0;
    int moved = 0, activated = 0;
    if (click_row_ok) {
        (void)ws_session_settings_select_row(s, WS_SET_ITEM_WIDTH);
        moved = ws_session_settings_cursor(s) == WS_SET_ITEM_WIDTH;
        /* A second click on the same row activates it (like Enter): the
         * resolution width steps up by one increment. */
        (void)ws_session_settings_select_row(s, WS_SET_ITEM_WIDTH);
        activated = ws_session_settings_item_value(s, WS_SET_ITEM_WIDTH) != width_before;
    }
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0); /* open */
    int w_py = -1;
    for (int y = 0; y < (int)opts->height && w_py < 0; ++y) {
        if (ws_hud_world_select_row_at(opts->width, opts->height, click_scale,
                                       (int)opts->width / 2, y) == WS_WORLD_SAMPLE) {
            w_py = y;
        }
    }
    int world_row_ok = w_py >= 0;
    int world_moved = 0;
    if (world_row_ok) {
        (void)ws_session_world_select_row(s, WS_WORLD_SAMPLE);
        world_moved = ws_session_world_select_cursor(s) == WS_WORLD_SAMPLE;
    }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0); /* close */
    (void)ws_session_apply_command(s, WS_CMD_TOGGLE_SETTINGS, 0.0);    /* close */
    char click_after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, click_after);
    int click_readonly = strcmp(click_before, click_after) == 0;
    int menu_click_ok = click_row_ok && moved && activated &&
                        world_row_ok && world_moved && click_readonly;
    printf("tour menu_click settings_row=%d moved=%d activated=%d "
           "world_row=%d world_moved=%d readonly=%d\n",
           click_row_ok, moved, activated, world_row_ok, world_moved, click_readonly);
    ok &= menu_click_ok;

    /* Step 15: free-camera ground collision (docs/17 "Kamera çarpışması ve
     * fiziksel engel farkındalığı"). Drive the free camera hard towards the
     * ground; with collision on the eye stops at the clearance floor above the
     * terrain, never sinking through it. Observer-only: the world digest is
     * untouched. The step captures the camera resting on the ground. */
    char collide_before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, collide_before);
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FREE, 0.0);
    int collide_on = ws_session_camera_collision(s);
    for (int i = 0; i < 60; ++i) {
        (void)ws_session_camera_input(s, 0.0, 0.0, -1.0, 0.0, 0.0, 0.1);
    }
    ws_camera cam = ws_session_camera(s);
    double ground = ws_session_camera_ground_height(s);
    int collision_ok = collide_on == 1 &&
                       cam.eye.y >= ground + WS_SESSION_CAM_CLEARANCE_M - 1e-6 &&
                       cam.eye.y <= ground + WS_SESSION_CAM_CLEARANCE_M + 1e-3;
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    char collide_after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, collide_after);
    int collide_readonly = strcmp(collide_before, collide_after) == 0;
    printf("tour camera_collision on=%d eye_y=%.3f ground=%.3f clearance=%.3f "
           "ok=%d readonly=%d\n",
           collide_on, cam.eye.y, ground, WS_SESSION_CAM_CLEARANCE_M,
           collision_ok, collide_readonly);
    ok &= collision_ok && collide_readonly;

    /* Step 16: solid-obstacle awareness (docs/17). Aim the free camera at a
     * real tree trunk and drive straight at it; the eye must stay outside the
     * trunk's keep radius the whole way and the world digest must be
     * untouched. The step captures the camera held at the trunk. */
    char tree_before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, tree_before);
    (void)ws_session_apply_command(s, WS_CMD_SET_VIEW_FREE, 0.0);
    uint32_t obs_n = ws_session_camera_obstacle_count(s);
    int tree_ok = obs_n > 0u;
    double tree_clear = 0.0;
    if (tree_ok) {
        ws_session_obstacle_view ob = ws_session_camera_obstacle(s, 0u);
        tree_ok = ob.valid == 1;
        if (tree_ok) {
            double keep = ob.radius + WS_SESSION_CAM_OBSTACLE_MARGIN_M;
            /* Start a few metres from the trunk, on a clear approach line, and
             * face the trunk (yaw so +x points at it). */
            (void)ws_session_set_free_camera(s, ob.x - 6.0, 6.0, ob.z, 0.0, 0.0);
            double min_gap = 1e9;
            for (int i = 0; i < 40; ++i) {
                (void)ws_session_camera_input(s, 1.0, 0.0, 0.0, 0.0, 0.0, 0.1);
                ws_camera cc = ws_session_camera(s);
                double ddx = cc.eye.x - ob.x;
                double ddz = cc.eye.z - ob.z;
                double gap = sqrt(ddx * ddx + ddz * ddz);
                if (gap < min_gap) { min_gap = gap; }
            }
            /* The eye never entered the trunk's keep radius. */
            tree_ok = min_gap >= keep - 1e-6;
            tree_clear = min_gap;
        }
    }
    ok &= capture_step(s, &fb, depth, opts, TOUR_STEPS[step], step); step++;
    char tree_after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, tree_after);
    int tree_readonly = strcmp(tree_before, tree_after) == 0;
    printf("tour tree_avoidance obstacles=%u min_gap=%.3f ok=%d readonly=%d\n",
           obs_n, tree_clear, tree_ok, tree_readonly);
    ok &= tree_ok && tree_readonly;

    if (mf != NULL) {
        fprintf(mf, "# Worldseed Engine P11 live tour manifest\n");
        fprintf(mf, "seed=%llu\n", (unsigned long long)opts->seed);
        fprintf(mf, "size=%ux%u\n", opts->width, opts->height);
        fprintf(mf, "world_tick=%llu\n", (unsigned long long)ws_session_tick(s));
        fprintf(mf, "population=%u\n", ws_session_total_population(s));
        fprintf(mf, "loaded_regions=%u\n", ws_session_loaded_regions(s));
        fprintf(mf, "save_load_roundtrip=%d\n", roundtrip);
        fprintf(mf, "shadows_verified=%d\n", shadow_pair_changed);
        fprintf(mf, "single_step_verified=%d\n", single_step_ok);
        fprintf(mf, "settings_menu_verified=%d\n", menu_changed && menu_readonly);
        fprintf(mf, "world_select_verified=%d\n", select_readonly && select_open == 1);
        fprintf(mf, "menu_click_verified=%d\n", menu_click_ok);
        fprintf(mf, "camera_collision_verified=%d\n", collision_ok && collide_readonly);
        fprintf(mf, "tree_avoidance_verified=%d\n", tree_ok && tree_readonly);
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

/* --- world-selection evidence (docs/17 "Ana deneyim") ------------------- */

static int run_worldselect(const ws_live_options *opts)
{
    if (ws_fs_make_dirs(opts->out_dir) != WS_OK) {
        fprintf(stderr, "live select: cannot create %s\n", opts->out_dir);
        return 1;
    }
    ws_session *s = NULL;
    if (ws_live_make_session(opts, &s) != WS_OK) {
        fprintf(stderr, "live select: session create failed\n");
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

    char checkpoint[512];
    snprintf(checkpoint, sizeof(checkpoint), "%s/checkpoint.wss", opts->out_dir);
    (void)ws_session_set_checkpoint_path(s, checkpoint);

    int ok = 1;
    int step = 0;

    /* Let the starting world run so the "new world" digest is a real, evolved
     * state (not the just-created state), and save a checkpoint so the "load"
     * choice becomes selectable. */
    for (int i = 0; i < 60; ++i) { (void)ws_session_step(s, 0.05); }
    int saved = ws_session_save(s, checkpoint) == WS_OK;
    ok &= saved;
    int can_load = ws_session_world_select_can_load(s);
    ok &= can_load;
    char new_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, new_digest);

    /* Open the selection screen (this is also what the application shows at
     * startup): three choices, the checkpoint present, the load row enabled. */
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0);
    int visible = ws_session_world_select_visible(s);
    ok &= visible == 1;
    ok &= ws_session_world_choice_enabled(s, WS_WORLD_LOAD) == 1;
    ok &= ws_session_world_choice_enabled(s, WS_WORLD_NEW) == 1;
    ok &= ws_session_world_choice_enabled(s, WS_WORLD_SAMPLE) == 1;
    ok &= capture_step(s, &fb, depth, opts, "world_select_screen", step); step++;

    /* Move the cursor down and back to prove navigation is real and bounded. */
    int c0 = ws_session_world_select_cursor(s);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, 1.0);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, 1.0);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, 1.0); /* clamps */
    int c_max = ws_session_world_select_cursor(s);
    ok &= c_max == WS_WORLD_CHOICE_COUNT - 1;
    ok &= c_max > c0;

    /* Choose "open the sample world": the seed changes to the documented sample
     * seed and the world is rebuilt fresh (tick 0, empty timeline). */
    (void)ws_session_world_confirm(s, WS_WORLD_SAMPLE);
    uint64_t sample_seed = ws_session_seed(s);
    int sample_fresh = ws_session_steps(s) == 0u && ws_session_timeline_count(s) == 0u;
    int sample_closed = ws_session_world_select_visible(s) == 0;
    ok &= sample_closed && sample_fresh;
    for (int i = 0; i < 40; ++i) { (void)ws_session_step(s, 0.05); }
    char sample_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, sample_digest);
    ok &= strcmp(sample_digest, new_digest) != 0;
    ok &= capture_step(s, &fb, depth, opts, "world_sample", step); step++;

    /* The sample world is reproducible: rebuild it and the same tick count gives
     * a bit-identical digest (determinism of "open the sample world"). */
    (void)ws_session_world_confirm(s, WS_WORLD_SAMPLE);
    for (int i = 0; i < 40; ++i) { (void)ws_session_step(s, 0.05); }
    char sample_digest2[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, sample_digest2);
    int sample_reproducible = strcmp(sample_digest, sample_digest2) == 0;
    ok &= sample_reproducible;

    /* Choose "load the existing world": the checkpoint we saved is restored
     * bit-exactly (digest matches the state when it was saved). */
    (void)ws_session_world_confirm(s, WS_WORLD_LOAD);
    char load_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, load_digest);
    int load_restored = strcmp(load_digest, new_digest) == 0;
    ok &= load_restored;
    ok &= capture_step(s, &fb, depth, opts, "world_load", step); step++;

    /* Choose "create a new world": a fresh seed, tick 0, empty timeline. */
    (void)ws_session_world_confirm(s, WS_WORLD_NEW);
    uint64_t fresh_seed = ws_session_seed(s);
    int new_fresh = ws_session_steps(s) == 0u && ws_session_timeline_count(s) == 0u;
    int new_seed_changed = fresh_seed != sample_seed;
    ok &= new_fresh && new_seed_changed;
    for (int i = 0; i < 40; ++i) { (void)ws_session_step(s, 0.05); }
    ok &= capture_step(s, &fb, depth, opts, "world_new", step); step++;

    /* The selection screen is observer-only bookkeeping: opening and closing it
     * without confirming leaves the world digest untouched. */
    (void)ws_session_world_confirm(s, WS_WORLD_NEW);
    for (int i = 0; i < 30; ++i) { (void)ws_session_step(s, 0.05); }
    char readonly_before[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, readonly_before);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_MOVE, 1.0);
    (void)ws_session_apply_command(s, WS_CMD_WORLD_SELECT_TOGGLE, 0.0);
    char readonly_after[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, readonly_after);
    int readonly = strcmp(readonly_before, readonly_after) == 0;
    ok &= readonly;

    char manifest[512];
    snprintf(manifest, sizeof(manifest), "%s/select-manifest.txt", opts->out_dir);
    FILE *mf = fopen(manifest, "w");
    if (mf != NULL) {
        fprintf(mf, "# Worldseed Engine P11 world-selection manifest\n");
        fprintf(mf, "seed=%llu\n", (unsigned long long)opts->seed);
        fprintf(mf, "size=%ux%u\n", opts->width, opts->height);
        fprintf(mf, "choices=%d\n", (int)WS_WORLD_CHOICE_COUNT);
        fprintf(mf, "can_load=%d\n", can_load);
        fprintf(mf, "cursor_max=%d\n", c_max);
        fprintf(mf, "sample_seed=%llu\n", (unsigned long long)sample_seed);
        fprintf(mf, "new_seed=%llu\n", (unsigned long long)fresh_seed);
        fprintf(mf, "sample_fresh=%d\n", sample_fresh);
        fprintf(mf, "sample_reproducible=%d\n", sample_reproducible);
        fprintf(mf, "load_restored=%d\n", load_restored);
        fprintf(mf, "new_fresh=%d\n", new_fresh);
        fprintf(mf, "readonly=%d\n", readonly);
        fprintf(mf, "new_digest=%s\n", new_digest);
        fprintf(mf, "sample_digest=%s\n", sample_digest);
        fprintf(mf, "load_digest=%s\n", load_digest);
        fprintf(mf, "steps=%d\n", step);
        fclose(mf);
    }

    free(depth);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    if (ok) {
        printf("LIVE_SELECT_OK choices=%d can_load=%d sample_reproducible=%d "
               "load_restored=%d readonly=%d steps=%d dir=%s\n",
               (int)WS_WORLD_CHOICE_COUNT, can_load, sample_reproducible,
               load_restored, readonly, step, opts->out_dir);
    }
    return ok ? 0 : 1;
}

/* --- export (docs/17 "Dışa aktarma") ------------------------------------ */

/* Export a run as a timestamped frame sequence plus the matching WAV: the exact
 * deliverable docs/17 specifies for evidence and later re-observation. The
 * frames are the live product's own renders (the same session, HUD and camera
 * the observer sees), written as zero-padded PPM files with an index that
 * records each frame's world time; the audio is the live procedural soundscape
 * mixed in step, so a birth or a kill heard in the WAV lines up with the frame
 * that shows it. A manifest ties the frames, the WAV digest and the world
 * digest together. The world is never modified by the export. */
static int run_export(const ws_live_options *opts)
{
    if (ws_fs_make_dirs(opts->out_dir) != WS_OK) {
        fprintf(stderr, "live export: cannot create %s\n", opts->out_dir);
        return 1;
    }
    ws_session *s = NULL;
    if (ws_live_make_session(opts, &s) != WS_OK) {
        fprintf(stderr, "live export: session create failed\n");
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

    const double dt = 1.0 / 30.0;                 /* export frame interval */
    const uint32_t frames_per_step = (uint32_t)(WS_AUDIO_SAMPLE_RATE * dt);
    int steps = (int)(opts->export_seconds / dt);
    if (steps < 1) {
        steps = 1;
    }
    const uint64_t total_audio_frames = (uint64_t)frames_per_step * (uint64_t)steps;
    int16_t *pcm = malloc((size_t)total_audio_frames * 2u * sizeof(int16_t));
    if (pcm == NULL) {
        free(depth);
        ws_framebuffer_free(&fb);
        ws_session_destroy(s);
        return 1;
    }

    char index_path[512];
    snprintf(index_path, sizeof(index_path), "%s/frames-index.txt", opts->out_dir);
    FILE *idx = fopen(index_path, "w");
    if (idx != NULL) {
        fprintf(idx, "# Worldseed Engine export frame index (docs/17)\n");
        fprintf(idx, "# file world_time_s world_tick frame_sha256\n");
    }

    int ok = 1;
    int written = 0;
    int nonzero_audio = 0;
    for (int i = 0; i < steps; ++i) {
        if (ws_session_step(s, dt) != WS_OK) {
            fprintf(stderr, "live export: step failed\n");
            ok = 0;
            break;
        }
        if (ws_session_render(s, &fb, depth) != WS_OK) {
            fprintf(stderr, "live export: render failed\n");
            ok = 0;
            break;
        }
        ws_hud_options ho = ws_hud_options_default();
        ho.scale = ws_session_hud_scale(s);
        ws_hud_draw(&fb, s, &ho);

        char path[512];
        snprintf(path, sizeof(path), "%s/frame-%05d.ppm", opts->out_dir, i);
        if (ws_framebuffer_write_ppm(&fb, path) != WS_OK) {
            fprintf(stderr, "live export: cannot write %s\n", path);
            ok = 0;
            break;
        }
        uint8_t digest[WS_SHA256_DIGEST_SIZE];
        char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
        ws_sha256_buffer(fb.pixels, (size_t)fb.width * fb.height * 4u, digest);
        ws_sha256_hex(digest, hex);
        if (idx != NULL) {
            fprintf(idx, "frame-%05d.ppm %.3f %llu %s\n", i,
                    ws_session_time_s(s), (unsigned long long)ws_session_tick(s), hex);
        }
        written++;

        /* Mix this frame interval's audio into its slot in the whole WAV, so
         * the exported soundscape spans exactly the exported frames. */
        int16_t *slot = &pcm[(size_t)i * frames_per_step * 2u];
        if (ws_session_render_audio(s, slot, frames_per_step) != WS_OK) {
            fprintf(stderr, "live export: audio mix failed\n");
            ok = 0;
            break;
        }
        if (!nonzero_audio) {
            for (uint32_t k = 0u; k < frames_per_step * 2u; ++k) {
                if (slot[k] != 0) { nonzero_audio = 1; break; }
            }
        }
    }
    if (idx != NULL) {
        fclose(idx);
    }

    char wav_path[512];
    snprintf(wav_path, sizeof(wav_path), "%s/soundscape.wav", opts->out_dir);
    uint64_t audio_frames_total = (uint64_t)frames_per_step * (uint64_t)written;
    if (ok && written > 0) {
        if (ws_audio_write_wav(wav_path, pcm, (uint32_t)audio_frames_total,
                               WS_AUDIO_SAMPLE_RATE) != WS_OK) {
            fprintf(stderr, "live export: cannot write %s\n", wav_path);
            ok = 0;
        }
    }

    char world_digest[WS_SESSION_DIGEST_HEX];
    (void)ws_session_digest(s, world_digest);

    char manifest[512];
    snprintf(manifest, sizeof(manifest), "%s/export-manifest.txt", opts->out_dir);
    FILE *mf = fopen(manifest, "w");
    if (mf != NULL) {
        fprintf(mf, "# Worldseed Engine export manifest (docs/17)\n");
        fprintf(mf, "seed=%llu\n", (unsigned long long)opts->seed);
        fprintf(mf, "size=%ux%u\n", opts->width, opts->height);
        fprintf(mf, "frames=%d\n", written);
        fprintf(mf, "frame_interval_s=%.4f\n", dt);
        fprintf(mf, "audio_sample_rate=%u\n", WS_AUDIO_SAMPLE_RATE);
        fprintf(mf, "audio_frames=%llu\n", (unsigned long long)audio_frames_total);
        fprintf(mf, "audio_nonzero=%d\n", nonzero_audio);
        fprintf(mf, "world_digest=%s\n", world_digest);
        fclose(mf);
    }

    free(pcm);
    free(depth);
    ws_framebuffer_free(&fb);
    ws_session_destroy(s);
    if (ok && written > 0 && nonzero_audio) {
        printf("LIVE_EXPORT_OK frames=%d wav=%s frames=%llu nonzero_audio=%d "
               "world_digest=%s dir=%s\n",
               written, wav_path, (unsigned long long)audio_frames_total,
               nonzero_audio, world_digest, opts->out_dir);
    }
    return (ok && written > 0) ? 0 : 1;
}

int ws_live_run(const ws_live_options *opts)
{
    if (opts == NULL || !opts->enabled) {
        return 2;
    }
    /* Options arrive already merged (parse_options resolves the settings file),
     * so the windowed and headless paths drive exactly the same product. */
    if (opts->export_mode) {
        return run_export(opts);
    }
    if (opts->tour) {
        return run_tour(opts);
    }
    if (opts->replay) {
        return run_replay(opts);
    }
    if (opts->worldselect) {
        return run_worldselect(opts);
    }
    if (opts->headless) {
        return run_headless(opts);
    }
    return ws_live_present(opts);
}
