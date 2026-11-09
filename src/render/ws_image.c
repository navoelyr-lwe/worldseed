/* Worldseed Engine — image comparison metrics. */
#include "ws_image.h"

#include <math.h>
#include <stddef.h>

#define WS_SSIM_C1 6.5025   /* (0.01 * 255)^2 */
#define WS_SSIM_C2 58.5225  /* (0.03 * 255)^2 */

static double channel(const ws_framebuffer *fb, size_t pixel, int c)
{
    return (double)fb->pixels[pixel * 4u + (size_t)c];
}

ws_result ws_image_compare(const ws_framebuffer *a, const ws_framebuffer *b,
                           ws_image_diff *out)
{
    if (a == NULL || b == NULL || out == NULL || a->pixels == NULL || b->pixels == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (a->width != b->width || a->height != b->height) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    size_t count = (size_t)a->width * (size_t)a->height;
    if (count == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    double sum_abs = 0.0;
    double max_abs = 0.0;
    double sum_sq = 0.0;
    uint64_t differing = 0u;

    for (size_t i = 0; i < count; ++i) {
        int pixel_differs = 0;
        for (int c = 0; c < 3; ++c) {
            double d = fabs(channel(a, i, c) - channel(b, i, c));
            sum_abs += d;
            if (d > max_abs) { max_abs = d; }
            sum_sq += d * d;
            if (d > 0.0) { pixel_differs = 1; }
        }
        if (pixel_differs) { differing += 1u; }
    }

    double samples = (double)(count * 3u);
    out->mean_abs_error = sum_abs / samples;
    out->max_abs_error = max_abs;
    out->differing_pixels = differing;
    double mse = sum_sq / samples;
    out->psnr_db = mse <= 0.0 ? INFINITY : 10.0 * log10((255.0 * 255.0) / mse);

    /* Mean SSIM over a 1x1 "window" approximation: global luminance/contrast
     * terms computed per channel. It is a coarse but stable regression signal. */
    double ssim_sum = 0.0;
    for (int c = 0; c < 3; ++c) {
        double ma = 0.0, mb = 0.0;
        for (size_t i = 0; i < count; ++i) {
            ma += channel(a, i, c);
            mb += channel(b, i, c);
        }
        ma /= (double)count;
        mb /= (double)count;
        double va = 0.0, vb = 0.0, cov = 0.0;
        for (size_t i = 0; i < count; ++i) {
            double da = channel(a, i, c) - ma;
            double db = channel(b, i, c) - mb;
            va += da * da;
            vb += db * db;
            cov += da * db;
        }
        va /= (double)count;
        vb /= (double)count;
        cov /= (double)count;
        double num = (2.0 * ma * mb + WS_SSIM_C1) * (2.0 * cov + WS_SSIM_C2);
        double den = (ma * ma + mb * mb + WS_SSIM_C1) * (va + vb + WS_SSIM_C2);
        ssim_sum += den == 0.0 ? 0.0 : num / den;
    }
    out->ssim = ssim_sum / 3.0;
    return WS_OK;
}

ws_result ws_image_coverage(const ws_framebuffer *fb, const uint8_t background[3],
                            int threshold, double *out_fraction)
{
    if (fb == NULL || fb->pixels == NULL || background == NULL || out_fraction == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    size_t count = (size_t)fb->width * (size_t)fb->height;
    if (count == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    uint64_t content = 0u;
    for (size_t i = 0; i < count; ++i) {
        int differs = 0;
        for (int c = 0; c < 3; ++c) {
            double d = fabs(channel(fb, i, c) - (double)background[c]);
            if (d > (double)threshold) {
                differs = 1;
                break;
            }
        }
        if (differs) { content += 1u; }
    }
    *out_fraction = (double)content / (double)count;
    return WS_OK;
}
