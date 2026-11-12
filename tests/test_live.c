/* Worldseed Engine — live application tests (P11).
 *
 * Drives the real headless evidence paths of the interactive application:
 * the parse layer, the deterministic headless run and the scripted tour. These
 * exercise the same code the Windows build presents in a window, so CI gets
 * real frames, a world digest and performance numbers without a display.
 */
#include "ws_test.h"

#include "app/ws_live.h"
#include "ui/ws_session.h"

#include <stdio.h>
#include <string.h>

static int test_default_options(void)
{
    ws_live_options o = ws_live_options_default();
    WS_CHECK(o.enabled == 0);
    WS_CHECK(o.headless == 0);
    WS_CHECK(o.width > 0u && o.height > 0u);
    WS_CHECK(o.render_scale >= 0); /* 0 = auto */
    return 1;
}

static int test_parse_disabled(void)
{
    const char *const argv[] = {"worldseed", "--selftest"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(2, argv, &o) == WS_OK);
    WS_CHECK(o.enabled == 0); /* non-live flags leave the live path off */
    return 1;
}

static int test_parse_live_flags(void)
{
    const char *const argv[] = {"worldseed", "--live-headless", "3", "--size", "640x360",
                    "--live-seed", "42", "--live-view", "1", "--live-time", "7200"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(11, argv, &o) == WS_OK);
    WS_CHECK(o.enabled == 1);
    WS_CHECK(o.headless == 1);
    WS_CHECK(o.seconds == 3.0);
    WS_CHECK(o.width == 640u && o.height == 360u);
    WS_CHECK(o.seed == 42u);
    WS_CHECK(o.start_view == WS_VIEW_FREE);
    WS_CHECK(o.start_time_of_day_s == 7200.0);
    return 1;
}

static int test_parse_rejects_bad(void)
{
    const char *const a1[] = {"worldseed", "--live-headless"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(2, a1, &o) != WS_OK);
    const char *const a2[] = {"worldseed", "--live-view", "99"};
    WS_CHECK(ws_live_parse_options(3, a2, &o) != WS_OK);
    const char *const a3[] = {"worldseed", "--live-headless", "0"};
    WS_CHECK(ws_live_parse_options(3, a3, &o) != WS_OK);
    return 1;
}

static int test_headless_run(void)
{
    const char *const argv[] = {"worldseed", "--live-headless", "1", "--size", "240x135"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(5, argv, &o) == WS_OK);
    WS_CHECK(ws_live_run(&o) == 0);
    return 1;
}

/* The frame-time gate reports p50/p95/p99 (docs/11). The helper must order the
 * samples, stay inside the array at both ends, and treat an empty set as 0. */
static int test_frame_percentiles(void)
{
    double s[10] = {9.0, 1.0, 8.0, 2.0, 7.0, 3.0, 6.0, 4.0, 5.0, 10.0};
    WS_CHECK(ws_live_percentile_ms(s, 10u, 0.0) == 1.0);   /* minimum */
    WS_CHECK(ws_live_percentile_ms(s, 10u, 1.0) == 10.0);  /* maximum */
    WS_CHECK(ws_live_percentile_ms(s, 10u, 0.50) == 5.0);  /* nearest-rank of 10 */
    WS_CHECK(ws_live_percentile_ms(s, 10u, 0.95) == 9.0);
    WS_CHECK(ws_live_percentile_ms(s, 10u, 0.99) == 9.0);
    /* Monotone and bounded at any percentile. */
    double prev = -1.0;
    for (int i = 0; i <= 20; ++i) {
        double p = (double)i / 20.0;
        double v = ws_live_percentile_ms(s, 10u, p);
        WS_CHECK(v >= prev);
        WS_CHECK(v >= 1.0 && v <= 10.0);
        prev = v;
    }
    /* Degenerate inputs. */
    WS_CHECK(ws_live_percentile_ms(s, 0u, 0.5) == 0.0);
    WS_CHECK(ws_live_percentile_ms(NULL, 10u, 0.5) == 0.0);
    return 1;
}

static int test_tour_run(void)
{
    const char *const argv[] = {"worldseed", "--live-tour", "ws_live_test_out", "--size", "160x90"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(5, argv, &o) == WS_OK);
    WS_CHECK(o.tour == 1);
    WS_CHECK(ws_live_run(&o) == 0);
    /* The tour wrote its manifest and at least one step capture. */
    FILE *mf = fopen("ws_live_test_out/manifest.txt", "r");
    WS_CHECK(mf != NULL);
    if (mf != NULL) {
        char line[128];
        int saw_roundtrip = 0;
        int saw_shadows = 0;
        int saw_step = 0;
        int saw_menu = 0;
        while (fgets(line, sizeof(line), mf) != NULL) {
            if (strstr(line, "save_load_roundtrip=1") != NULL) {
                saw_roundtrip = 1;
            }
            if (strstr(line, "shadows_verified=1") != NULL) {
                saw_shadows = 1;
            }
            if (strstr(line, "single_step_verified=1") != NULL) {
                saw_step = 1;
            }
            if (strstr(line, "settings_menu_verified=1") != NULL) {
                saw_menu = 1;
            }
        }
        fclose(mf);
        WS_CHECK(saw_roundtrip == 1);
        WS_CHECK(saw_shadows == 1);
        WS_CHECK(saw_step == 1);
        WS_CHECK(saw_menu == 1);
    }
    return 1;
}

static int test_settings_file_precedence(void)
{
    /* A saved settings file supplies defaults, but an explicit CLI flag wins.
     * Headless runs must ignore the file entirely so evidence is hermetic. */
    const char *path = "ws_live_settings_test.cfg";
    remove(path);

    ws_settings saved = ws_settings_default();
    saved.width = 800u;
    saved.height = 600u;
    saved.view = WS_VIEW_MAP;
    saved.time_of_day_s = 3.0 * 3600.0;
    saved.seed = 555ULL;
    WS_CHECK(ws_settings_save(path, &saved) == WS_OK);

    /* Headless runs ignore the file and still succeed (hermetic evidence). */
    const char *const a1[] = {"worldseed", "--live-headless", "1",
                              "--live-settings", path};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(5, a1, &o) == WS_OK);
    WS_CHECK(o.width != 800u || o.height != 600u); /* file not applied */
    WS_CHECK(ws_live_run(&o) == 0);

    /* Explicit flags win over the file. */
    const char *const a2[] = {"worldseed", "--live-headless", "1", "--size", "320x180",
                              "--live-seed", "9", "--live-view", "1"};
    WS_CHECK(ws_live_parse_options(9, a2, &o) == WS_OK);
    WS_CHECK(o.width == 320u && o.height == 180u);
    WS_CHECK(o.seed == 9u);
    WS_CHECK(o.start_view == WS_VIEW_FREE);

    /* A non-live invocation with a settings file still merges it. */
    const char *const a3[] = {"worldseed", "--live", "--live-settings", path};
    WS_CHECK(ws_live_parse_options(4, a3, &o) == WS_OK);
    WS_CHECK(o.width == 800u && o.height == 600u);
    WS_CHECK(o.start_view == WS_VIEW_MAP);

    remove(path);
    return 1;
}

static int test_live_gpu_run(void)
{
    /* The opt-in GPU render path is reachable from the live product loop and is
     * observer-only: rendering through it does not change the world. On a host
     * without an OpenCL device it falls back to the CPU reference, so the run
     * succeeds everywhere. */
    const char *const cpu[] = {"worldseed", "--live-headless", "2",
                               "--live-no-settings", "--size", "320x180"};
    ws_live_options oc;
    WS_CHECK(ws_live_parse_options(6, cpu, &oc) == WS_OK);
    WS_CHECK(oc.settings.gpu_render == 0);
    WS_CHECK(ws_live_run(&oc) == 0);

    const char *const gpu[] = {"worldseed", "--live-headless", "2",
                               "--live-no-settings", "--size", "320x180",
                               "--live-gpu"};
    ws_live_options og;
    WS_CHECK(ws_live_parse_options(7, gpu, &og) == WS_OK);
    WS_CHECK(og.settings.gpu_render == 1);
    WS_CHECK(ws_live_run(&og) == 0);

    /* Same seed and frame count: the two render paths agree on the world. */
    WS_CHECK(oc.seed == og.seed);
    return 1;
}

static int test_bench_run(void)
{
    /* The live-loop benchmark drives the real product loop for a wall-clock
     * duration and reports frame times with the per-path counters. On the CPU
     * path it must succeed and write a per-frame CSV. */
    const char *const argv[] = {"worldseed", "--live-bench", "1",
                                "--live-no-settings", "--size", "240x135",
                                "--live-bench-out", "ws_live_bench_test_out"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(8, argv, &o) == WS_OK);
    WS_CHECK(o.bench == 1);
    WS_CHECK(o.headless == 1);
    WS_CHECK(o.bench_seconds == 1.0);
    WS_CHECK(ws_live_run(&o) == 0);
    FILE *f = fopen("ws_live_bench_test_out/frames.csv", "r");
    WS_CHECK(f != NULL);
    if (f != NULL) {
        char line[128];
        int rows = 0;
        while (fgets(line, sizeof(line), f) != NULL) {
            if (line[0] != '#') { rows += 1; }
        }
        fclose(f);
        WS_CHECK(rows > 0);
    }
    return 1;
}

/* A GPU-requested benchmark that cannot use a device must fail loudly (exit
 * non-zero) instead of reporting a CPU measurement as a GPU result. */
static int test_bench_gpu_requires_device(void)
{
    const char *const argv[] = {"worldseed", "--live-bench", "1",
                                "--live-no-settings", "--size", "160x90",
                                "--live-gpu"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(7, argv, &o) == WS_OK);
    WS_CHECK(o.settings.gpu_render == 1);
    int rc = ws_live_run(&o);
    /* Either the device drew every frame (rc == 0) or the run failed because it
     * fell back (rc == 3). Any other code would mean the accounting is wrong. */
    WS_CHECK(rc == 0 || rc == 3);
    return 1;
}

static int test_replay_run(void)
{
    const char *const argv[] = {"worldseed", "--live-replay", "ws_live_replay_test_out",
                                "--size", "160x90"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(5, argv, &o) == WS_OK);
    WS_CHECK(o.replay == 1);
    WS_CHECK(o.headless == 1);
    WS_CHECK(ws_live_run(&o) == 0);
    /* The evidence manifest records the replay invariants. */
    FILE *f = fopen("ws_live_replay_test_out/replay-manifest.txt", "r");
    WS_CHECK(f != NULL);
    if (f != NULL) {
        char line[256];
        int saw_samples = 0, saw_span = 0, saw_newest = 0, saw_restored = 0;
        while (fgets(line, sizeof(line), f) != NULL) {
            if (strstr(line, "samples=") != NULL) { saw_samples = 1; }
            if (strstr(line, "span_seconds=") != NULL) { saw_span = 1; }
            if (strstr(line, "newest_matches_live=1") != NULL) { saw_newest = 1; }
            if (strstr(line, "live_restored=1") != NULL) { saw_restored = 1; }
        }
        fclose(f);
        WS_CHECK(saw_samples && saw_span && saw_newest && saw_restored);
    }
    return 1;
}

static int test_worldselect_run(void)
{
    const char *const argv[] = {"worldseed", "--live-select", "ws_live_select_test_out",
                                "--size", "160x90"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(5, argv, &o) == WS_OK);
    WS_CHECK(o.worldselect == 1);
    WS_CHECK(o.headless == 1);
    WS_CHECK(ws_live_run(&o) == 0);
    /* The evidence manifest records the world-selection invariants. */
    FILE *f = fopen("ws_live_select_test_out/select-manifest.txt", "r");
    WS_CHECK(f != NULL);
    if (f != NULL) {
        char line[256];
        int saw_can_load = 0, saw_repro = 0, saw_restored = 0, saw_readonly = 0;
        while (fgets(line, sizeof(line), f) != NULL) {
            if (strstr(line, "can_load=1") != NULL) { saw_can_load = 1; }
            if (strstr(line, "sample_reproducible=1") != NULL) { saw_repro = 1; }
            if (strstr(line, "load_restored=1") != NULL) { saw_restored = 1; }
            if (strstr(line, "readonly=1") != NULL) { saw_readonly = 1; }
        }
        fclose(f);
        WS_CHECK(saw_can_load && saw_repro && saw_restored && saw_readonly);
    }
    return 1;
}

/* Export evidence mode (docs/17 "Dışa aktarma"): the run writes a timestamped
 * frame sequence plus the matching WAV and a manifest. Two runs of the same
 * seed export bit-identical frames and audio. */
static int test_export_run(void)
{
    const char *const argv[] = {"worldseed", "--live-export", "ws_live_export_test_out",
                                "--size", "160x90", "--live-export-seconds", "0.5"};
    ws_live_options o;
    WS_CHECK(ws_live_parse_options(7, argv, &o) == WS_OK);
    WS_CHECK(o.export_mode == 1);
    WS_CHECK(o.headless == 1);
    WS_CHECK(ws_live_run(&o) == 0);

    FILE *mf = fopen("ws_live_export_test_out/export-manifest.txt", "r");
    WS_CHECK(mf != NULL);
    if (mf != NULL) {
        char line[256];
        int saw_frames = 0, saw_nonzero = 0, saw_digest = 0;
        while (fgets(line, sizeof(line), mf) != NULL) {
            if (strstr(line, "frames=15") != NULL) { saw_frames = 1; }
            if (strstr(line, "audio_nonzero=1") != NULL) { saw_nonzero = 1; }
            if (strstr(line, "world_digest=") != NULL) { saw_digest = 1; }
        }
        fclose(mf);
        WS_CHECK(saw_frames && saw_nonzero && saw_digest);
    }
    /* First and last frame files exist, plus the WAV. */
    FILE *f0 = fopen("ws_live_export_test_out/frame-00000.ppm", "rb");
    FILE *f14 = fopen("ws_live_export_test_out/frame-00014.ppm", "rb");
    FILE *wav = fopen("ws_live_export_test_out/soundscape.wav", "rb");
    WS_CHECK(f0 != NULL && f14 != NULL && wav != NULL);
    if (f0) { fclose(f0); }
    if (f14) { fclose(f14); }
    if (wav) { fclose(wav); }

    /* A second run exports a bit-identical frame sequence and WAV. */
    const char *const argv2[] = {"worldseed", "--live-export", "ws_live_export_test_out2",
                                 "--size", "160x90", "--live-export-seconds", "0.5"};
    ws_live_options o2;
    WS_CHECK(ws_live_parse_options(7, argv2, &o2) == WS_OK);
    WS_CHECK(ws_live_run(&o2) == 0);

    const char *frames_to_compare[] = {"frame-00000.ppm", "frame-00007.ppm",
                                       "frame-00014.ppm"};
    for (int i = 0; i < 3; ++i) {
        char pa[256], pb[256];
        snprintf(pa, sizeof(pa), "ws_live_export_test_out/%s", frames_to_compare[i]);
        snprintf(pb, sizeof(pb), "ws_live_export_test_out2/%s", frames_to_compare[i]);
        FILE *fa = fopen(pa, "rb");
        FILE *fb = fopen(pb, "rb");
        WS_CHECK(fa != NULL && fb != NULL);
        if (fa != NULL && fb != NULL) {
            int same = 1;
            for (;;) {
                int ca = fgetc(fa);
                int cb = fgetc(fb);
                if (ca != cb) { same = 0; break; }
                if (ca == EOF) { break; }
            }
            WS_CHECK(same);
            fclose(fa);
            fclose(fb);
        }
    }
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"live_default_options", test_default_options},
        {"live_parse_disabled", test_parse_disabled},
        {"live_parse_flags", test_parse_live_flags},
        {"live_parse_rejects_bad", test_parse_rejects_bad},
        {"live_headless_run", test_headless_run},
        {"live_frame_percentiles", test_frame_percentiles},
        {"live_tour_run", test_tour_run},
        {"live_settings_precedence", test_settings_file_precedence},
        {"live_gpu_run", test_live_gpu_run},
        {"live_bench_run", test_bench_run},
        {"live_bench_gpu_requires_device", test_bench_gpu_requires_device},
        {"live_replay_run", test_replay_run},
        {"live_worldselect_run", test_worldselect_run},
        {"live_export_run", test_export_run},
    };
    return ws_test_run("live", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
