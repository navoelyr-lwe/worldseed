/* Worldseed Engine — image comparison metrics for golden-scene regression.
 *
 * Dependency-free, C17. Technical metrics only: they catch wrong geometry,
 * missing content or backend differences. They do not prove aesthetic quality;
 * the quality protocol in docs/08 also requires reference targets and review.
 */
#ifndef WS_RENDER_IMAGE_H
#define WS_RENDER_IMAGE_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_framebuffer.h"

typedef struct ws_image_diff {
    double mean_abs_error;   /* 0..255, average per-channel absolute difference */
    double max_abs_error;    /* 0..255 */
    double psnr_db;          /* peak signal-to-noise ratio; +inf when identical */
    double ssim;             /* mean structural similarity, 0..1 (1 = identical) */
    uint64_t differing_pixels; /* pixels where any channel differs */
} ws_image_diff;

/* Compare two same-size RGBA8 buffers. Returns WS_ERR_INVALID_ARGUMENT on size
 * mismatch or NULL input. */
ws_result ws_image_compare(const ws_framebuffer *a, const ws_framebuffer *b,
                           ws_image_diff *out);

/* Fraction of pixels that are not fully background-free: used as a coverage
 * sanity check to detect empty or blank frames. `threshold` is a per-channel
 * absolute difference from the background colour that counts as "content". */
ws_result ws_image_coverage(const ws_framebuffer *fb, const uint8_t background[3],
                            int threshold, double *out_fraction);

#endif /* WS_RENDER_IMAGE_H */
