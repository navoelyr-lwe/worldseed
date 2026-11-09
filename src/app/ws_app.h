/* Worldseed Engine — application entry and headless capture.
 *
 * The application renders the shared showcase scenes (ws_showcase) through the
 * CPU reference renderer and presents the resulting pixel buffer. On Windows it
 * opens a Win32 window and blits the buffer with GDI (presentation only, not a
 * renderer). Everywhere else, and with --headless/--capture/--selftest, it
 * runs without a window so CI can produce real frame evidence.
 *
 * This is the P2 user-visible preview: a single rendered frame per scene with a
 * simple viewer. It is not the final interactive product.
 */
#ifndef WS_APP_H
#define WS_APP_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "../render/ws_framebuffer.h"

typedef struct ws_app_options {
    int list_scenes;          /* print scene ids and exit */
    int headless;             /* never open a window */
    int selftest;             /* render every scene, verify, print result, exit */
    int capture;              /* write a PPM for the selected scene and exit */
    int scene_index;          /* scene to display or capture */
    uint64_t seed;            /* scene seed */
    uint32_t width;           /* render width */
    uint32_t height;          /* render height */
    const char *capture_path; /* output path for --capture */
} ws_app_options;

/* Parse command-line arguments into `opts` (defaults filled in first).
 * Returns WS_OK, or WS_ERR_INVALID_ARGUMENT on an unknown flag. */
ws_result ws_app_parse_options(int argc, char **argv, ws_app_options *opts);

/* Render scene `scene_index` into `fb` at the options' resolution. The
 * framebuffer must already be created at that size. */
ws_result ws_app_render_scene(const ws_app_options *opts, ws_framebuffer *fb,
                              float *depth);

/* Run the application. Opens a window unless headless. Returns a process exit
 * code (0 on success). */
int ws_app_run(const ws_app_options *opts);

/* Platform presentation hook. Opens a window and presents a live-rendered
 * frame; returns the process exit code. Implemented per platform. */
int ws_app_present_win32(const ws_app_options *opts);

#endif /* WS_APP_H */
