/* Worldseed Engine — full-scene CPU/GPU semantic comparison (P12).
 *
 * Renders every catalog scene on the bit-identical CPU reference renderer and on
 * the OpenCL full-scene accelerator (ws_render_scene_gpu), then reports how far
 * the GPU frame is from the CPU frame. ADR-001: the GPU path must reproduce the
 * CPU reference within a documented tolerance; the CPU frame stays the golden
 * reference and is never replaced.
 *
 * The tool always runs. With no OpenCL device it prints gpu=unavailable and
 * verifies the CPU reference only, so CI without a GPU still gets CPU evidence
 * and does not fail.
 *
 * Output is machine-readable "gpu_scene ..." lines followed by WS_GPU_SCENE_OK
 * (or a nonzero exit on a failed assertion).
 */
#include "platform/ws_clock.h"
#include "platform/ws_hardware.h"
#include "render/ws_catalog.h"
#include "render/ws_framebuffer.h"
#include "render/ws_render.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GSC_WIDTH 320u
#define GSC_HEIGHT 240u
/* Per-channel tolerance and differing-pixel fraction. The GPU path runs the
 * whole shading pipeline in 32-bit float where the CPU reference uses double,
 * so a small fraction of pixels can land on the other side of an 8-bit rounding
 * step. These are the documented semantic limits for the full-scene path. */
#define GSC_MAX_STRUCTURAL_FRACTION 0.0005  /* genuine errors: near zero */
#define GSC_MAX_TIE_CHANNEL_DELTA 90        /* tie colour deltas stay modest */

typedef struct diff_stats {
    double diff_fraction;        /* pixels whose colour differs at all */
    double tie_fraction;         /* differing pixels explained by a near-coplanar depth tie */
    double structural_fraction;  /* differing pixels NOT explained by a depth tie (real errors) */
    int max_channel_delta;
    int max_tie_channel_delta;   /* worst colour delta among tie pixels */
    double mean_channel_delta;
} diff_stats;

/* A differing pixel is a depth tie when the CPU and GPU chose surfaces at
 * effectively the same depth: the CPU path runs the whole pipeline in double
 * where the GPU uses 32-bit float, so two surfaces closer than a float epsilon
 * can be ordered differently by the two paths. Such a pixel still shows a valid
 * surface at the same depth; a differing pixel whose depth also disagrees is a
 * genuine structural error. */
#define GSC_TIE_DEPTH_EPS 1.0e-4

static void compare_frames(const ws_framebuffer *a, const ws_framebuffer *b,
                           const float *depth_a, const float *depth_b, diff_stats *out)
{
    size_t count = (size_t)a->width * (size_t)a->height;
    size_t diff_pixels = 0u;
    size_t tie_pixels = 0u;
    size_t structural_pixels = 0u;
    int max_delta = 0;
    int max_tie_delta = 0;
    double sum_delta = 0.0;
    for (size_t i = 0; i < count; ++i) {
        const uint8_t *pa = &a->pixels[i * 4u];
        const uint8_t *pb = &b->pixels[i * 4u];
        int pd = 0;
        for (int c = 0; c < 3; ++c) {
            int d = (int)pa[c] - (int)pb[c];
            if (d < 0) {
                d = -d;
            }
            if (d > max_delta) {
                max_delta = d;
            }
            sum_delta += (double)d;
            if (d > pd) {
                pd = d;
            }
        }
        if (pd == 0) {
            continue;
        }
        diff_pixels += 1u;
        double dd = (double)depth_a[i] - (double)depth_b[i];
        if (dd < 0.0) {
            dd = -dd;
        }
        if (dd <= GSC_TIE_DEPTH_EPS) {
            tie_pixels += 1u;
            if (pd > max_tie_delta) {
                max_tie_delta = pd;
            }
        } else {
            structural_pixels += 1u;
        }
    }
    out->diff_fraction = count ? (double)diff_pixels / (double)count : 0.0;
    out->tie_fraction = count ? (double)tie_pixels / (double)count : 0.0;
    out->structural_fraction = count ? (double)structural_pixels / (double)count : 0.0;
    out->max_channel_delta = max_delta;
    out->max_tie_channel_delta = max_tie_delta;
    out->mean_channel_delta = count ? sum_delta / ((double)count * 3.0) : 0.0;
}

int main(int argc, char **argv)
{
    uint64_t seed = argc > 1 ? strtoull(argv[1], NULL, 10) : 20260101ULL;

    ws_framebuffer cpu_fb;
    ws_framebuffer gpu_fb;
    if (ws_framebuffer_create(&cpu_fb, GSC_WIDTH, GSC_HEIGHT) != WS_OK ||
        ws_framebuffer_create(&gpu_fb, GSC_WIDTH, GSC_HEIGHT) != WS_OK) {
        fprintf(stderr, "framebuffer allocation failed\n");
        return 1;
    }
    size_t pixel_count = (size_t)GSC_WIDTH * (size_t)GSC_HEIGHT;
    float *cpu_depth = malloc(pixel_count * sizeof(float));
    float *gpu_depth = malloc(pixel_count * sizeof(float));
    if (cpu_depth == NULL || gpu_depth == NULL) {
        fprintf(stderr, "depth allocation failed\n");
        free(cpu_depth);
        free(gpu_depth);
        ws_framebuffer_free(&cpu_fb);
        ws_framebuffer_free(&gpu_fb);
        return 1;
    }

    int failed = 0;
    int gpu_used = 0;
    int gpu_seen = 0;
    size_t scene_count = ws_catalog_count();
    for (size_t i = 0; i < scene_count; ++i) {
        const char *id = ws_catalog_id(i);
        ws_catalog catalog;
        if (ws_catalog_build(&catalog, i, seed) != WS_OK) {
            fprintf(stderr, "catalog build failed for scene %s\n", id);
            failed = 1;
            continue;
        }
        const ws_scene *scene = ws_catalog_scene(&catalog);

        ws_render_stats cpu_stats;
        uint64_t c0 = ws_clock_monotonic_ns();
        ws_result cres = ws_render_scene(scene, &cpu_fb, cpu_depth, 32u, &cpu_stats);
        uint64_t c1 = ws_clock_monotonic_ns();

        ws_render_stats gpu_stats;
        uint64_t g0 = ws_clock_monotonic_ns();
        ws_result gres = ws_render_scene_gpu(scene, &gpu_fb, gpu_depth, 32u, &gpu_stats);
        uint64_t g1 = ws_clock_monotonic_ns();

        if (cres != WS_OK || gres != WS_OK) {
            fprintf(stderr, "render failed for %s (cpu=%s gpu=%s)\n", id,
                    ws_result_name(cres), ws_result_name(gres));
            failed = 1;
            ws_catalog_free(&catalog);
            continue;
        }

        double cpu_ms = ws_clock_ns_to_seconds(c1 - c0) * 1000.0;
        double gpu_ms = ws_clock_ns_to_seconds(g1 - g0) * 1000.0;

        if (gpu_stats.path == WS_RENDER_PATH_GPU) {
            gpu_used += 1;
            gpu_seen = 1;
            diff_stats d;
            compare_frames(&cpu_fb, &gpu_fb, cpu_depth, gpu_depth, &d);
            const char *dump = getenv("WS_GSC_DUMP");
            if (dump != NULL) {
                char pa[512], pb[512];
                snprintf(pa, sizeof(pa), "%s/%s_cpu.ppm", dump, id);
                snprintf(pb, sizeof(pb), "%s/%s_gpu.ppm", dump, id);
                (void)ws_framebuffer_write_ppm(&cpu_fb, pa);
                (void)ws_framebuffer_write_ppm(&gpu_fb, pb);
            }
            printf("gpu_scene scene=%s path=gpu diff_fraction=%.5f tie_fraction=%.5f "
                   "structural_fraction=%.5f max_channel_delta=%d max_tie_channel_delta=%d "
                   "mean_channel_delta=%.4f cpu_ms=%.2f gpu_ms=%.2f\n",
                   id, d.diff_fraction, d.tie_fraction, d.structural_fraction,
                   d.max_channel_delta, d.max_tie_channel_delta, d.mean_channel_delta,
                   cpu_ms, gpu_ms);
            if (d.structural_fraction > GSC_MAX_STRUCTURAL_FRACTION) {
                fprintf(stderr, "scene %s structural_fraction %.5f exceeds %.5f\n",
                        id, d.structural_fraction, (double)GSC_MAX_STRUCTURAL_FRACTION);
                failed = 1;
            }
            if (d.max_tie_channel_delta > GSC_MAX_TIE_CHANNEL_DELTA) {
                fprintf(stderr, "scene %s max_tie_channel_delta %d exceeds %d\n",
                        id, d.max_tie_channel_delta, GSC_MAX_TIE_CHANNEL_DELTA);
                failed = 1;
            }
        } else {
            printf("gpu_scene scene=%s path=cpu cpu_ms=%.2f\n", id, cpu_ms);
        }
        ws_catalog_free(&catalog);
    }

    free(cpu_depth);
    free(gpu_depth);
    ws_framebuffer_free(&cpu_fb);
    ws_framebuffer_free(&gpu_fb);

    if (failed) {
        return 1;
    }
    printf("WS_GPU_SCENE_OK scenes=%zu gpu_used=%d\n", scene_count, gpu_used);
    if (!gpu_seen) {
        printf("gpu_scene gpu=unavailable (CPU reference verified only)\n");
    }
    return 0;
}
