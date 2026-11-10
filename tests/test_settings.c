/* Worldseed Engine — persistent settings tests (P11, docs/17 item 9).
 *
 * The settings file is the user's persistent configuration. These tests pin the
 * contract: defaults are sane, a round-trip through text is lossless, malformed
 * or unknown input never crashes and never corrupts valid fields, and every
 * out-of-range value is clamped instead of rejected. The settings are
 * configuration only and never touch the simulation, so there is nothing to
 * assert about the world here.
 */
#include "ws_test.h"

#include "platform/ws_settings.h"

#include <stdio.h>
#include <string.h>

static int test_defaults_sane(void)
{
    ws_settings s = ws_settings_default();
    WS_CHECK(s.version == WS_SETTINGS_VERSION);
    WS_CHECK(s.width == 1280u);
    WS_CHECK(s.height == 720u);
    WS_CHECK(s.view == 0);
    WS_CHECK(s.render_scale == 0);
    WS_CHECK(s.shadows == 0);
    WS_CHECK(s.fullscreen == 0);
    /* Defaults must already be within range. */
    ws_settings copy = s;
    ws_settings_clamp(&copy);
    WS_CHECK(memcmp(&copy, &s, sizeof(s)) == 0);
    return 1;
}

static int test_roundtrip(void)
{
    ws_settings in = ws_settings_default();
    in.seed = 424242ULL;
    in.width = 1920u;
    in.height = 1080u;
    in.view = 2;
    in.time_of_day_s = 19.5 * 3600.0;
    in.render_scale = 2;
    in.fullscreen = 1;
    in.shadows = 1;
    in.stats = 1;
    in.help = 1;

    char buf[512];
    size_t n = ws_settings_format(buf, sizeof(buf), &in);
    WS_CHECK(n > 0u);
    WS_CHECK(strlen(buf) == n);

    ws_settings out;
    WS_CHECK(ws_settings_parse(buf, &out) == WS_OK);
    WS_CHECK(out.seed == in.seed);
    WS_CHECK(out.width == in.width);
    WS_CHECK(out.height == in.height);
    WS_CHECK(out.view == in.view);
    WS_CHECK(out.time_of_day_s == in.time_of_day_s);
    WS_CHECK(out.render_scale == in.render_scale);
    WS_CHECK(out.fullscreen == in.fullscreen);
    WS_CHECK(out.shadows == in.shadows);
    WS_CHECK(out.stats == in.stats);
    WS_CHECK(out.help == in.help);
    return 1;
}

static int test_malformed_and_unknown(void)
{
    /* Garbage, comments, blank lines, unknown keys and a key without '=' must
     * all be tolerated; the valid keys around them must still take effect. */
    const char *text =
        "# a comment\n"
        "\n"
        "   \n"
        "width=800\n"
        "bogus_key=99\n"
        "no_equals_here\n"
        "height=not_a_number\n"
        "view=1\n"
        "=orphan_value\n";
    ws_settings s;
    WS_CHECK(ws_settings_parse(text, &s) == WS_OK);
    WS_CHECK(s.width == 800u);
    WS_CHECK(s.view == 1);
    /* A malformed number leaves the default in place (720), not zero. */
    WS_CHECK(s.height == 720u);
    return 1;
}

static int test_clamping(void)
{
    const char *text =
        "width=0\n"
        "height=999999\n"
        "view=7\n"
        "render_scale=9\n"
        "time_of_day=999999\n"
        "fullscreen=5\n"
        "shadows=-3\n";
    ws_settings s;
    WS_CHECK(ws_settings_parse(text, &s) == WS_OK);
    WS_CHECK(s.width == 16u);
    WS_CHECK(s.height == 16384u);
    WS_CHECK(s.view == 0);
    WS_CHECK(s.render_scale == 0);
    WS_CHECK(s.time_of_day_s == 0.0);
    WS_CHECK(s.fullscreen == 1);
    WS_CHECK(s.shadows == 1);
    return 1;
}

static int test_truncated_format(void)
{
    ws_settings in = ws_settings_default();
    char small[8];
    size_t want = ws_settings_format(small, sizeof(small), &in);
    WS_CHECK(want >= sizeof(small)); /* reports full length like snprintf */
    WS_CHECK(strlen(small) == sizeof(small) - 1u); /* still NUL-terminated */
    return 1;
}

static char *s_scratch(void)
{
    static char buf[512];
    return buf;
}

static int test_null_safety(void)
{
    ws_settings s;
    WS_CHECK(ws_settings_parse(NULL, &s) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_settings_parse("width=800\n", NULL) == WS_ERR_NULL_POINTER);
    /* snprintf-like: a NULL buffer with zero capacity still reports the length
     * without writing (and without crashing). */
    char buf[512];
    size_t full = ws_settings_format(buf, sizeof(buf), &s);
    WS_CHECK(ws_settings_format(NULL, 0u, &s) == full);
    WS_CHECK(ws_settings_format(s_scratch(), 8u, NULL) == 0u);
    WS_CHECK(ws_settings_load(NULL, &s) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_settings_save("ws_settings_null_test.cfg", NULL) ==
             WS_ERR_NULL_POINTER);
    ws_settings_clamp(NULL); /* must not crash */
    return 1;
}

static int test_file_roundtrip(void)
{
    const char *path = "ws_settings_test.cfg";
    remove(path);
    /* A missing file yields defaults and is not an error. */
    ws_settings missing;
    WS_CHECK(ws_settings_load(path, &missing) == WS_OK);
    WS_CHECK(missing.width == 1280u);

    ws_settings in = ws_settings_default();
    in.seed = 777ULL;
    in.width = 1600u;
    in.height = 900u;
    in.view = 3;
    in.shadows = 1;
    WS_CHECK(ws_settings_save(path, &in) == WS_OK);

    ws_settings out;
    WS_CHECK(ws_settings_load(path, &out) == WS_OK);
    WS_CHECK(out.seed == in.seed);
    WS_CHECK(out.width == in.width);
    WS_CHECK(out.height == in.height);
    WS_CHECK(out.view == in.view);
    WS_CHECK(out.shadows == in.shadows);
    remove(path);
    return 1;
}

int main(void)
{
    const ws_test_case cases[] = {
        {"defaults sane", test_defaults_sane},
        {"roundtrip", test_roundtrip},
        {"malformed and unknown", test_malformed_and_unknown},
        {"clamping", test_clamping},
        {"truncated format", test_truncated_format},
        {"null safety", test_null_safety},
        {"file roundtrip", test_file_roundtrip},
    };
    return ws_test_run("settings", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
