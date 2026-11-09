/* Worldseed Engine — image diff for golden-scene regression.
 *
 * Compares two PPM frames and gates on tolerance, not equality: mean per-channel
 * absolute difference and structural similarity. This tolerates a few
 * edge pixels flipping between platforms/compilers (small mean shift) while
 * still catching real regressions such as missing or misplaced geometry (large
 * mean shift, low SSIM). Peak difference is reported for diagnosis but not
 * gated, because a single anti-aliased edge pixel can be large.
 *
 * Usage: ws_imgdiff <a.ppm> <b.ppm> [max_mean] [min_ssim]
 * Defaults: max_mean = 3.0, min_ssim = 0.99. Exits non-zero when exceeded.
 */
#include "render/ws_framebuffer.h"
#include "render/ws_image.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <a.ppm> <b.ppm> [max_mean] [min_ssim]\n", argv[0]);
        return 2;
    }
    double max_mean = argc > 3 ? strtod(argv[3], NULL) : 3.0;
    double min_ssim = argc > 4 ? strtod(argv[4], NULL) : 0.99;

    ws_framebuffer a;
    ws_framebuffer b;
    ws_result ra = ws_framebuffer_read_ppm(&a, argv[1]);
    if (ra != WS_OK) {
        fprintf(stderr, "cannot read %s: %s\n", argv[1], ws_result_name(ra));
        return 2;
    }
    ws_result rb = ws_framebuffer_read_ppm(&b, argv[2]);
    if (rb != WS_OK) {
        fprintf(stderr, "cannot read %s: %s\n", argv[2], ws_result_name(rb));
        ws_framebuffer_free(&a);
        return 2;
    }

    ws_image_diff diff;
    ws_result rc = ws_image_compare(&a, &b, &diff);
    if (rc != WS_OK) {
        fprintf(stderr, "compare failed: %s\n", ws_result_name(rc));
        ws_framebuffer_free(&a);
        ws_framebuffer_free(&b);
        return 2;
    }

    printf("image_diff mean=%.4f peak=%.4f psnr=%.2f ssim=%.6f differing=%llu\n",
           diff.mean_abs_error, diff.max_abs_error, diff.psnr_db, diff.ssim,
           (unsigned long long)diff.differing_pixels);

    /* Diagnostic: signed mean shift and a couple of sample pixels, so a
     * platform-specific difference can be located without the raw frames. */
    {
        double sum[3] = {0.0, 0.0, 0.0};
        size_t count = (size_t)a.width * (size_t)a.height;
        for (size_t i = 0; i < count; ++i) {
            for (int c = 0; c < 3; ++c) {
                sum[c] += (double)a.pixels[i * 4u + (size_t)c] - (double)b.pixels[i * 4u + (size_t)c];
            }
        }
        printf("signed_mean a-b: r=%.3f g=%.3f b=%.3f\n",
               sum[0] / (double)count, sum[1] / (double)count, sum[2] / (double)count);
        size_t probes[3] = {0u, count / 2u, count - 1u};
        for (int p = 0; p < 3; ++p) {
            size_t i = probes[p];
            printf("pixel[%zu] a=(%u,%u,%u) b=(%u,%u,%u)\n", i,
                   a.pixels[i * 4u], a.pixels[i * 4u + 1u], a.pixels[i * 4u + 2u],
                   b.pixels[i * 4u], b.pixels[i * 4u + 1u], b.pixels[i * 4u + 2u]);
        }
    }

    int ok = diff.mean_abs_error <= max_mean && diff.ssim >= min_ssim;
    if (!ok) {
        fprintf(stderr, "tolerance exceeded: mean<=%.3f ssim>=%.4f\n", max_mean, min_ssim);
    }
    ws_framebuffer_free(&a);
    ws_framebuffer_free(&b);
    if (!ok) {
        return 1;
    }
    printf("WS_IMGDIFF_OK\n");
    return 0;
}
