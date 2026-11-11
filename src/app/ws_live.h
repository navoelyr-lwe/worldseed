/* Worldseed Engine — interactive live application (P11).
 *
 * This is the user-visible product: a full-screen, real-time view of a living
 * world. It owns a ws_session (the world, ecology, director, audio and camera)
 * and a ws_hud (the read-outs), renders frames with the CPU reference renderer
 * and presents them with the platform layer (Win32/GDI blit on Windows, which is
 * presentation only, not a renderer).
 *
 * The same module runs headless for automated evidence: `--live-headless N`
 * steps the world for N seconds and prints frame timing and a state digest, and
 * `--live-tour DIR` drives a deterministic scripted tour (view switches, animal
 * selection, save/load) and writes a PNG/PPM capture plus a manifest for each
 * step, so CI has real frames, a digest and performance numbers to check.
 */
#ifndef WS_APP_LIVE_H
#define WS_APP_LIVE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "../platform/ws_settings.h"
#include "../render/ws_framebuffer.h"
#include "../ui/ws_session.h"

/* Which fields the user set explicitly on the command line. The settings file
 * supplies the rest, so an explicit flag always wins over the saved value. */
enum {
    WS_LIVE_SET_NONE = 0u,
    WS_LIVE_SET_SEED = 1u << 0,
    WS_LIVE_SET_SIZE = 1u << 1,
    WS_LIVE_SET_VIEW = 1u << 2,
    WS_LIVE_SET_TIME = 1u << 3,
    WS_LIVE_SET_RENDER_SCALE = 1u << 4,
    WS_LIVE_SET_FULLSCREEN = 1u << 5
};

typedef struct ws_live_options {
    int enabled;        /* a --live* flag was present */
    int headless;       /* never open a window */
    int fullscreen;     /* start full-screen (windowed mode) */
    double seconds;     /* headless/tour simulated duration */
    int tour;           /* run the scripted evidence tour */
    int replay;         /* run the replay evidence mode */
    int worldselect;    /* run the world-selection evidence mode */
    int export_mode;    /* run the frame-sequence + WAV export (docs/17) */
    double export_seconds; /* export duration, world seconds */
    int no_select;      /* do not show the startup world-selection screen */
    const char *out_dir; /* directory for tour captures (NULL = none) */
    const char *capture_path; /* single capture path (NULL = none) */
    uint64_t seed;
    uint32_t width;
    uint32_t height;
    int start_view;     /* ws_view_mode */
    double start_time_of_day_s;
    int render_scale;   /* windowed internal resolution divisor (0 = auto) */
    const char *settings_path; /* persistent settings file (NULL = none) */
    unsigned set_flags; /* WS_LIVE_SET_* bits for explicit CLI values */
    ws_settings settings; /* merged settings (file + CLI) applied to the session */
} ws_live_options;

ws_live_options ws_live_options_default(void);

/* Parse live-specific arguments. Returns WS_OK with opts->enabled == 0 when no
 * live flag is present (the caller then falls through to the showcase app), or
 * WS_ERR_INVALID_ARGUMENT on a malformed live flag. */
ws_result ws_live_parse_options(int argc, const char *const *argv, ws_live_options *opts);

/* Build the session described by `opts` (world, ecology, director, audio and
 * camera). Shared by the headless/tour paths and the windowed presentation so
 * both drive exactly the same product. */
ws_result ws_live_make_session(const ws_live_options *opts, ws_session **out);

/* Run the live application. Returns a process exit code. */
int ws_live_run(const ws_live_options *opts);

/* Derive a fresh "new world" seed from `base` and the wall clock (deterministic
 * 64-bit value; the world is fully reproducible from the seed the user sees). */
uint64_t ws_live_new_world_seed(uint64_t base);

/* Show the startup world-selection screen on a freshly created session (docs/17
 * "Ana deneyim"): the user chooses new / load / sample when the application
 * opens. No-op when opts->no_select is set. The cursor defaults to "load" when
 * a checkpoint exists, otherwise to "new". */
void ws_live_startup_world_select(const ws_live_options *opts, ws_session *s);

/* Persist the observer's settings (view, clock, panels, shadows) plus the
 * window fields, to opts->settings_path. Called on a clean windowed exit; a
 * NULL settings_path or session is a no-op. */
void ws_live_save_settings(const ws_live_options *opts, const ws_session *s);

/* Sorted-percentile helper for the frame-time gate (docs/11). Sorts a copy of
 * the caller's samples in place and returns the value at the given percentile
 * (0.0..1.0) using the nearest-rank index; returns 0 for an empty set. Used by
 * the headless performance report and its unit test. */
double ws_live_percentile_ms(double *samples, size_t count, double p);

/* Platform presentation hook (full-screen windowed). */
int ws_live_present(const ws_live_options *opts);

#endif /* WS_APP_LIVE_H */
