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
        while (fgets(line, sizeof(line), mf) != NULL) {
            if (strstr(line, "save_load_roundtrip=1") != NULL) {
                saw_roundtrip = 1;
            }
        }
        fclose(mf);
        WS_CHECK(saw_roundtrip == 1);
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
        {"live_tour_run", test_tour_run},
    };
    return ws_test_run("live", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
