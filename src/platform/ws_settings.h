/* Worldseed Engine — persistent application settings (P11, docs/17 item 9).
 *
 * A tiny, dependency-free key=value configuration so the user's choices
 * (resolution, start view, time of day, render scale, shadows, panel
 * visibility, seed) survive between runs. The format is plain text with one
 * `key=value` per line; unknown keys are ignored so an older build can read a
 * newer file, and out-of-range values are clamped rather than rejected.
 *
 * This is configuration only: it never changes the authoritative simulation.
 * Loading or saving settings is a no-op for the world state.
 */
#ifndef WS_PLATFORM_SETTINGS_H
#define WS_PLATFORM_SETTINGS_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"

#define WS_SETTINGS_VERSION 1

typedef struct ws_settings {
    int version;
    uint64_t seed;
    uint32_t width;
    uint32_t height;
    int view;               /* 0=auto 1=free 2=follow 3=map */
    double time_of_day_s;   /* seconds since midnight, 0..86400 */
    int render_scale;       /* 0=auto, 1..4 fixed divisor */
    int fullscreen;         /* 0/1 */
    int shadows;            /* 0/1 directional shadows */
    int stats;              /* 0/1 statistics panel visible */
    int help;               /* 0/1 help panel visible */
} ws_settings;

/* Sensible defaults matching the live application's own defaults. */
ws_settings ws_settings_default(void);

/* Clamp every field into its valid range. Applied automatically by parse and
 * load; exposed so a caller mutating the struct can re-normalise it. */
void ws_settings_clamp(ws_settings *s);

/* Parse `key=value` text into `out`. `out` is first reset to the defaults, so
 * a missing key keeps its default. Returns WS_ERR_INVALID_ARGUMENT only on a
 * NULL argument; malformed lines and unknown keys are skipped. */
ws_result ws_settings_parse(const char *text, ws_settings *out);

/* Format `s` as canonical text. Writes at most `cap` bytes (always
 * NUL-terminated when cap > 0) and returns the number of bytes that would be
 * written excluding the terminator, like snprintf. */
size_t ws_settings_format(char *buf, size_t cap, const ws_settings *s);

/* Read settings from `path`. A missing file is not an error: `out` is left at
 * the defaults and WS_OK is returned, so a first run just works. Returns
 * WS_ERR_NULL_POINTER on NULL args. */
ws_result ws_settings_load(const char *path, ws_settings *out);

/* Write settings to `path` atomically-ish (write then rename is left to the
 * caller's platform if needed; here a plain rewrite). Returns WS_ERR_IO when
 * the file cannot be opened or written. */
ws_result ws_settings_save(const char *path, const ws_settings *s);

#endif /* WS_PLATFORM_SETTINGS_H */
