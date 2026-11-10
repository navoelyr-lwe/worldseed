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

#include <stdint.h>

#include "../core/ws_result.h"
#include "../render/ws_framebuffer.h"
#include "../ui/ws_session.h"

typedef struct ws_live_options {
    int enabled;        /* a --live* flag was present */
    int headless;       /* never open a window */
    int fullscreen;     /* start full-screen (windowed mode) */
    double seconds;     /* headless/tour simulated duration */
    int tour;           /* run the scripted evidence tour */
    const char *out_dir; /* directory for tour captures (NULL = none) */
    const char *capture_path; /* single capture path (NULL = none) */
    uint64_t seed;
    uint32_t width;
    uint32_t height;
    int start_view;     /* ws_view_mode */
    double start_time_of_day_s;
    int render_scale;   /* windowed internal resolution divisor (0 = auto) */
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

/* Platform presentation hook (full-screen windowed). */
int ws_live_present(const ws_live_options *opts);

#endif /* WS_APP_LIVE_H */
