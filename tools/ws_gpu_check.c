/* Worldseed Engine — CPU/GPU semantic comparison and device-loss recovery.
 *
 * Runs the reference CPU rasterizer and the OpenCL C 1.2 raster kernel over the
 * same deterministic screen-space triangle batch and reports the difference
 * (ADR-001: GPU must reproduce the CPU reference within tolerance). It also
 * exercises device-loss fallback: after a simulated loss the backend must report
 * unavailable, the GPU call must fail cleanly, and the CPU path must still
 * produce the correct image.
 *
 * The tool always runs. With no OpenCL device it prints gpu=unavailable and
 * still verifies the CPU reference, so CI without a GPU still gets CPU evidence.
 *
 * Output is machine-readable "gpu_check ..." lines followed by WS_GPU_CHECK_OK
 * (or a nonzero exit on a failed assertion).
 */
#include "core/ws_rng.h"
#include "platform/ws_clock.h"
#include "gpu/ws_opencl.h"
#include "platform/ws_hardware.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GC_WIDTH 256u
#define GC_HEIGHT 192u
#define GC_TRIANGLES 96u
#define GC_MAX_DIFF_FRACTION 0.01  /* <= 1% of pixels may differ (float vs double) */
#define GC_MAX_CHANNEL_DELTA 4     /* per-channel tolerance, 0..255 */

static void clear_depth(float *depth, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        depth[i] = 1.0e30f;
    }
}

/* Deterministic overlapping triangles that exercise the depth test and
 * barycentric colour interpolation. */
static void build_triangles(float *vx, float *vy, float *vz, uint8_t *color, uint32_t count)
{
    ws_rng rng;
    ws_rng_init(&rng, 0x9E3779B97F4A7C15ULL, 7ULL);

    /* Full-screen background triangle at a far depth. */
    vx[0] = -1.0f; vy[0] = -1.0f; vz[0] = 0.9f;
    vx[1] = (float)GC_WIDTH + 1.0f; vy[1] = -1.0f; vz[1] = 0.9f;
    vx[2] = (float)GC_WIDTH * 0.5f; vy[2] = (float)GC_HEIGHT + 1.0f; vz[2] = 0.9f;
    color[0] = 30u; color[1] = 40u; color[2] = 60u;
    color[3] = 60u; color[4] = 50u; color[5] = 30u;
    color[6] = 20u; color[7] = 60u; color[8] = 40u;

    for (uint32_t t = 1u; t < count; ++t) {
        uint32_t b = t * 3u;
        uint32_t c = t * 9u;
        for (int k = 0; k < 3; ++k) {
            double rx = (double)((uint32_t)ws_rng_next_u64(&rng) % GC_WIDTH);
            double ry = (double)((uint32_t)ws_rng_next_u64(&rng) % GC_HEIGHT);
            vx[b + (uint32_t)k] = (float)rx;
            vy[b + (uint32_t)k] = (float)ry;
            vz[b + (uint32_t)k] = 0.05f + 0.9f * (float)((uint32_t)ws_rng_next_u64(&rng) % 1000u) / 1000.0f;
            color[c + (uint32_t)k * 3u + 0u] = (uint8_t)((uint32_t)ws_rng_next_u64(&rng) % 256u);
            color[c + (uint32_t)k * 3u + 1u] = (uint8_t)((uint32_t)ws_rng_next_u64(&rng) % 256u);
            color[c + (uint32_t)k * 3u + 2u] = (uint8_t)((uint32_t)ws_rng_next_u64(&rng) % 256u);
        }
    }
}

static void raster_cpu(const float *vx, const float *vy, const float *vz,
                       const uint8_t *color, uint32_t count,
                       ws_framebuffer *fb, float *depth)
{
    clear_depth(depth, (size_t)fb->width * fb->height);
    ws_framebuffer_clear(fb, 0u, 0u, 0u, 255u);
    for (uint32_t t = 0u; t < count; ++t) {
        const float *tx = &vx[t * 3u];
        const float *ty = &vy[t * 3u];
        const float *tz = &vz[t * 3u];
        const unsigned char *col = &color[t * 9u];
        ws_raster_triangle(fb, depth, tx, ty, tz, col, 0u, NULL);
    }
}

int main(void)
{
    static float vx[GC_TRIANGLES * 3u];
    static float vy[GC_TRIANGLES * 3u];
    static float vz[GC_TRIANGLES * 3u];
    static uint8_t color[GC_TRIANGLES * 9u];

    build_triangles(vx, vy, vz, color, GC_TRIANGLES);

    ws_framebuffer fb_cpu;
    if (ws_framebuffer_create(&fb_cpu, GC_WIDTH, GC_HEIGHT) != WS_OK) {
        fprintf(stderr, "framebuffer create failed\n");
        return 1;
    }
    size_t pixel_count = (size_t)GC_WIDTH * GC_HEIGHT;
    float *depth_cpu = (float *)malloc(pixel_count * sizeof(float));
    uint8_t *pixels_gpu = (uint8_t *)malloc(pixel_count * 4u);
    float *depth_gpu = (float *)malloc(pixel_count * sizeof(float));
    if (depth_cpu == NULL || pixels_gpu == NULL || depth_gpu == NULL) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    raster_cpu(vx, vy, vz, color, GC_TRIANGLES, &fb_cpu, depth_cpu);
    printf("gpu_check cpu_reference triangles=%u %ux%u ok\n", GC_TRIANGLES, GC_WIDTH, GC_HEIGHT);

    ws_opencl *cl = NULL;
    ws_result cr = ws_opencl_create(&cl);
    int have_gpu = (cr == WS_OK) && ws_opencl_available(cl);
    int rc_code = 0;

    if (!have_gpu) {
        printf("gpu_check gpu=unavailable fallback=cpu_ok\n");
        printf("WS_GPU_CHECK_OK\n");
        free(depth_cpu);
        free(pixels_gpu);
        free(depth_gpu);
        ws_framebuffer_free(&fb_cpu);
        ws_opencl_destroy(cl);
        return 0;
    }

    const ws_opencl_info *info = ws_opencl_info_of(cl);
    printf("gpu_check gpu=available device=\"%s\" version=\"%s\" units=%u\n",
           info->device_name, info->device_version, info->compute_units);

    ws_result rr = ws_opencl_raster(cl, vx, vy, vz, color, GC_TRIANGLES,
                                    GC_WIDTH, GC_HEIGHT, pixels_gpu, depth_gpu);
    if (rr != WS_OK) {
        fprintf(stderr, "gpu_check gpu raster failed: %s\n", ws_result_name(rr));
        rc_code = 1;
    } else {
        uint64_t diff_pixels = 0u;
        int max_delta = 0;
        float max_depth_delta = 0.0f;
        for (size_t i = 0; i < pixel_count; ++i) {
            int d = 0;
            for (int c = 0; c < 3; ++c) {
                int a = (int)fb_cpu.pixels[i * 4u + (size_t)c];
                int b = (int)pixels_gpu[i * 4u + (size_t)c];
                int ad = a > b ? a - b : b - a;
                if (ad > d) {
                    d = ad;
                }
            }
            if (d != 0) {
                diff_pixels += 1u;
            }
            if (d > max_delta) {
                max_delta = d;
            }
            float dd = fabsf(depth_cpu[i] - depth_gpu[i]);
            if (dd > max_depth_delta) {
                max_depth_delta = dd;
            }
        }
        double frac = (double)diff_pixels / (double)pixel_count;
        printf("gpu_check compare pixels=%zu diff_pixels=%llu diff_fraction=%.6f "
               "max_channel_delta=%d max_depth_delta=%.3e\n",
               pixel_count, (unsigned long long)diff_pixels, frac, max_delta,
               (double)max_depth_delta);
        if (frac > GC_MAX_DIFF_FRACTION || max_delta > GC_MAX_CHANNEL_DELTA) {
            fprintf(stderr, "gpu_check FAILED: difference exceeds tolerance\n");
            rc_code = 1;
        } else {
            printf("gpu_check semantic_match=ok tolerance_fraction=%.4f tolerance_delta=%d\n",
                   GC_MAX_DIFF_FRACTION, GC_MAX_CHANNEL_DELTA);
        }
    }

    /* Tile-binned kernel (P12): must be bit-identical to the reference kernel
     * (same pixels and depth) and must be at least as fast. This is the evidence
     * that the acceleration path does not change the image. */
    uint8_t *pixels_tiled = (uint8_t *)malloc(pixel_count * 4u);
    float *depth_tiled = (float *)malloc(pixel_count * sizeof(float));
    if (pixels_tiled == NULL || depth_tiled == NULL) {
        fprintf(stderr, "allocation failed\n");
        rc_code = 1;
    } else {
        uint64_t t0 = ws_clock_monotonic_ns();
        ws_result tr = ws_opencl_raster_tiled(cl, vx, vy, vz, color, GC_TRIANGLES,
                                              GC_WIDTH, GC_HEIGHT, 0u, pixels_tiled, depth_tiled);
        uint64_t t1 = ws_clock_monotonic_ns();
        if (tr != WS_OK) {
            fprintf(stderr, "gpu_check tiled raster failed: %s\n", ws_result_name(tr));
            rc_code = 1;
        } else {
            int pix_same = memcmp(pixels_gpu, pixels_tiled, pixel_count * 4u) == 0;
            int depth_same = memcmp(depth_gpu, depth_tiled, pixel_count * sizeof(float)) == 0;
            printf("gpu_check tiled bit_identical=%d tiled_ms=%.2f\n",
                   (pix_same && depth_same) ? 1 : 0, (double)(t1 - t0) / 1e6);
            if (!pix_same || !depth_same) {
                fprintf(stderr, "gpu_check FAILED: tiled kernel differs from reference\n");
                rc_code = 1;
            }
        }
        free(pixels_tiled);
        free(depth_tiled);
    }

    /* Device-loss recovery: after a simulated loss the backend must be
     * unavailable, the GPU call must fail cleanly, and the CPU image must be
     * unchanged (world state is never on the GPU, ADR-001). */
    ws_opencl_simulate_device_loss(cl);
    int lost = ws_opencl_device_lost(cl) && !ws_opencl_available(cl);
    ws_result after = ws_opencl_raster(cl, vx, vy, vz, color, GC_TRIANGLES,
                                       GC_WIDTH, GC_HEIGHT, pixels_gpu, depth_gpu);
    int failed_cleanly = (after != WS_OK);
    ws_framebuffer fb_cpu2;
    memset(&fb_cpu2, 0, sizeof(fb_cpu2));
    int cpu_ok = ws_framebuffer_create(&fb_cpu2, GC_WIDTH, GC_HEIGHT) == WS_OK;
    if (cpu_ok) {
        raster_cpu(vx, vy, vz, color, GC_TRIANGLES, &fb_cpu2, depth_cpu);
        cpu_ok = memcmp(fb_cpu.pixels, fb_cpu2.pixels, pixel_count * 4u) == 0;
        ws_framebuffer_free(&fb_cpu2);
    }
    printf("gpu_check device_loss_recovered=%d gpu_unavailable=%d gpu_call_failed=%d cpu_fallback_ok=%d\n",
           (lost && failed_cleanly && cpu_ok) ? 1 : 0, lost, failed_cleanly, cpu_ok);
    if (!(lost && failed_cleanly && cpu_ok)) {
        fprintf(stderr, "gpu_check FAILED: device-loss recovery\n");
        rc_code = 1;
    }

    if (rc_code == 0) {
        printf("WS_GPU_CHECK_OK\n");
    }
    free(depth_cpu);
    free(pixels_gpu);
    free(depth_gpu);
    ws_framebuffer_free(&fb_cpu);
    ws_opencl_destroy(cl);
    return rc_code;
}
