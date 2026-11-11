/* Worldseed Engine — GPU and performance scaling tests (P10).
 *
 * Covers the P10 acceptance points that can be checked in-process:
 *   - the multi-core scheduler is deterministic (parallel result == serial
 *     result, byte for byte) and its worker count is bounded by the hardware;
 *   - hardware discovery never fails and reports sane values;
 *   - the OpenCL backend either is absent (CPU fallback, not an error) or
 *     reproduces the CPU rasterizer within tolerance on the same batch;
 *   - the tile-binned OpenCL kernel (P12) is bit-identical to the reference
 *     kernel, and the host tile bin lists every triangle whose bounding box
 *     covers a pixel (so the tiled kernel can never drop a covering triangle);
 *   - a device loss makes the backend unavailable and the CPU path still works.
 * The GPU cases skip cleanly (return pass) when no OpenCL device is present, so
 * the suite is meaningful on a GPU-less machine too.
 */
#include "ws_test.h"

#include "core/ws_sched.h"
#include "core/ws_rng.h"
#include "gpu/ws_opencl.h"
#include "gpu/ws_tilebin.h"
#include "platform/ws_hardware.h"
#include "platform/ws_threads.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* --- scheduler ----------------------------------------------------------- */
typedef struct sum_ctx {
    const int *input;
    uint64_t *sink;
} sum_ctx;

static void sum_body(void *p, uint32_t i)
{
    sum_ctx *c = (sum_ctx *)p;
    /* A pure function of the index: writes only its own slot. */
    uint64_t v = (uint64_t)(uint32_t)c->input[i] * 2654435761u;
    c->sink[i] = v ^ (v >> 13);
}

static int test_scheduler_determinism(void)
{
    enum { N = 100000 };
    static int input[N];
    static uint64_t serial[N];
    static uint64_t parallel[N];
    for (int i = 0; i < N; ++i) {
        input[i] = (i * 37 + 11) % 9973;
    }

    sum_ctx ctx;
    ctx.input = input;
    ctx.sink = serial;
    for (int i = 0; i < N; ++i) {
        sum_body(&ctx, (uint32_t)i);
    }

    ws_sched *sched = NULL;
    WS_CHECK(ws_sched_create_default(&sched) == WS_OK);
    uint32_t hw = ws_thread_hardware_concurrency();
    if (hw > 0u) {
        WS_CHECK(ws_sched_worker_count(sched) <= hw - 1u);
    }

    ctx.sink = parallel;
    ws_sched_parallel_for(sched, 0u, N, sum_body, &ctx);
    WS_CHECK(memcmp(serial, parallel, sizeof(serial)) == 0);
    WS_CHECK(ws_sched_jobs_dispatched(sched) == 1u);

    /* A second dispatch must reuse the pool and stay deterministic. */
    memset(parallel, 0, sizeof(parallel));
    ws_sched_parallel_for(sched, 0u, N, sum_body, &ctx);
    WS_CHECK(memcmp(serial, parallel, sizeof(serial)) == 0);

    ws_sched_destroy(sched);

    /* The zero-worker pool is the deterministic serial fallback. */
    ws_sched *serial_pool = NULL;
    WS_CHECK(ws_sched_create(&serial_pool, 0u) == WS_OK);
    WS_CHECK(ws_sched_worker_count(serial_pool) == 0u);
    memset(parallel, 0, sizeof(parallel));
    ws_sched_parallel_for(serial_pool, 0u, N, sum_body, &ctx);
    WS_CHECK(memcmp(serial, parallel, sizeof(serial)) == 0);
    ws_sched_destroy(serial_pool);
    return 1;
}

static int test_scheduler_empty_range(void)
{
    ws_sched *sched = NULL;
    WS_CHECK(ws_sched_create(&sched, 2u) == WS_OK);
    sum_ctx ctx;
    static uint64_t sink[4];
    static const int input[4] = {1, 2, 3, 4};
    ctx.input = input;
    ctx.sink = sink;
    /* end <= begin is a no-op and must not dispatch a job. */
    ws_sched_parallel_for(sched, 5u, 5u, sum_body, &ctx);
    ws_sched_parallel_for(sched, 5u, 2u, sum_body, &ctx);
    WS_CHECK(ws_sched_jobs_dispatched(sched) == 0u);
    ws_sched_destroy(sched);
    return 1;
}

/* --- hardware ------------------------------------------------------------ */
static int test_hardware_probe(void)
{
    ws_hardware_info hw;
    WS_CHECK(ws_hardware_probe(&hw) == WS_OK);
    WS_CHECK(hw.os_name[0] != '\0');
    /* OpenCL absence must not be reported as an error: device_found is 0/1. */
    WS_CHECK(hw.opencl.device_found == 0 || hw.opencl.device_found == 1);

    char desc[512];
    ws_hardware_describe(&hw, desc, sizeof(desc));
    WS_CHECK(strstr(desc, "cpu_threads=") != NULL);
    WS_CHECK(strstr(desc, "os=") != NULL);
    return 1;
}

/* --- GPU semantic comparison and device loss ----------------------------- */
#define GT_W 128u
#define GT_H 96u
#define GT_TRI 48u

static void build_small_batch(float *vx, float *vy, float *vz, uint8_t *color)
{
    for (uint32_t t = 0; t < GT_TRI; ++t) {
        uint32_t b = t * 3u;
        uint32_t c = t * 9u;
        float fx = (float)((t * 53u) % GT_W);
        float fy = (float)((t * 29u) % GT_H);
        for (int k = 0; k < 3; ++k) {
            vx[b + (uint32_t)k] = fx + (float)((t + (uint32_t)k * 7u) % 20u) - 10.0f;
            vy[b + (uint32_t)k] = fy + (float)((t + (uint32_t)k * 5u) % 20u) - 10.0f;
            vz[b + (uint32_t)k] = 0.1f + 0.8f * (float)((t * 3u + (uint32_t)k) % 100u) / 100.0f;
            color[c + (uint32_t)k * 3u + 0u] = (uint8_t)((t * 11u) % 256u);
            color[c + (uint32_t)k * 3u + 1u] = (uint8_t)((t * 23u) % 256u);
            color[c + (uint32_t)k * 3u + 2u] = (uint8_t)((t * 31u) % 256u);
        }
    }
}

static int test_gpu_semantics_and_loss(void)
{
    static float vx[GT_TRI * 3u], vy[GT_TRI * 3u], vz[GT_TRI * 3u];
    static uint8_t color[GT_TRI * 9u];
    build_small_batch(vx, vy, vz, color);

    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, GT_W, GT_H) == WS_OK);
    size_t count = (size_t)GT_W * GT_H;
    float *depth = (float *)malloc(count * sizeof(float));
    uint8_t *gpu_pixels = (uint8_t *)malloc(count * 4u);
    float *gpu_depth = (float *)malloc(count * sizeof(float));
    WS_CHECK(depth != NULL && gpu_pixels != NULL && gpu_depth != NULL);

    for (size_t i = 0; i < count; ++i) {
        depth[i] = 1.0e30f;
    }
    ws_framebuffer_clear(&fb, 0u, 0u, 0u, 255u);
    for (uint32_t t = 0; t < GT_TRI; ++t) {
        ws_raster_triangle(&fb, depth, &vx[t * 3u], &vy[t * 3u], &vz[t * 3u],
                           &color[t * 9u], 0u, NULL);
    }

    ws_opencl *cl = NULL;
    ws_result cr = ws_opencl_create(&cl);
    if (cr != WS_OK || !ws_opencl_available(cl)) {
        /* No device: the CPU fallback is the correct, non-error outcome. */
        printf("    (no OpenCL device; CPU fallback path)\n");
        free(depth);
        free(gpu_pixels);
        free(gpu_depth);
        ws_framebuffer_free(&fb);
        return 1;
    }

    WS_CHECK(ws_opencl_raster(cl, vx, vy, vz, color, GT_TRI, GT_W, GT_H,
                              gpu_pixels, gpu_depth) == WS_OK);

    /* The tile-binned kernel must be bit-identical to the reference kernel:
     * same pixels and same depth, byte for byte. This is the ADR-001 guarantee
     * for the P12 acceleration path. */
    uint8_t *tiled_pixels = (uint8_t *)malloc(count * 4u);
    float *tiled_depth = (float *)malloc(count * sizeof(float));
    WS_CHECK(tiled_pixels != NULL && tiled_depth != NULL);
    WS_CHECK(ws_opencl_raster_tiled(cl, vx, vy, vz, color, GT_TRI, GT_W, GT_H, 0u,
                                    tiled_pixels, tiled_depth) == WS_OK);
    WS_CHECK(memcmp(gpu_pixels, tiled_pixels, count * 4u) == 0);
    WS_CHECK(memcmp(gpu_depth, tiled_depth, count * sizeof(float)) == 0);
    /* A different tile size must still be bit-identical (binning is a filter). */
    WS_CHECK(ws_opencl_raster_tiled(cl, vx, vy, vz, color, GT_TRI, GT_W, GT_H, 8u,
                                    tiled_pixels, tiled_depth) == WS_OK);
    WS_CHECK(memcmp(gpu_pixels, tiled_pixels, count * 4u) == 0);
    free(tiled_pixels);
    free(tiled_depth);

    uint64_t diff = 0u;
    for (size_t i = 0; i < count; ++i) {
        int d0 = (int)fb.pixels[i * 4u + 0u] - (int)gpu_pixels[i * 4u + 0u];
        int d1 = (int)fb.pixels[i * 4u + 1u] - (int)gpu_pixels[i * 4u + 1u];
        int d2 = (int)fb.pixels[i * 4u + 2u] - (int)gpu_pixels[i * 4u + 2u];
        if (d0 < 0) { d0 = -d0; }
        if (d1 < 0) { d1 = -d1; }
        if (d2 < 0) { d2 = -d2; }
        if (d0 > 4 || d1 > 4 || d2 > 4) {
            diff += 1u;
        }
    }
    WS_CHECK(diff * 100u <= count); /* <= 1% of pixels may differ */

    /* Device loss: backend goes unavailable, the GPU call fails cleanly and the
     * CPU image is unchanged. */
    ws_opencl_simulate_device_loss(cl);
    WS_CHECK(ws_opencl_device_lost(cl) == 1);
    WS_CHECK(ws_opencl_available(cl) == 0);
    WS_CHECK(ws_opencl_raster(cl, vx, vy, vz, color, GT_TRI, GT_W, GT_H,
                              gpu_pixels, gpu_depth) != WS_OK);

    ws_framebuffer fb2;
    WS_CHECK(ws_framebuffer_create(&fb2, GT_W, GT_H) == WS_OK);
    float *depth2 = (float *)malloc(count * sizeof(float));
    WS_CHECK(depth2 != NULL);
    for (size_t i = 0; i < count; ++i) {
        depth2[i] = 1.0e30f;
    }
    ws_framebuffer_clear(&fb2, 0u, 0u, 0u, 255u);
    for (uint32_t t = 0; t < GT_TRI; ++t) {
        ws_raster_triangle(&fb2, depth2, &vx[t * 3u], &vy[t * 3u], &vz[t * 3u],
                           &color[t * 9u], 0u, NULL);
    }
    WS_CHECK(memcmp(fb.pixels, fb2.pixels, count * 4u) == 0);

    free(depth2);
    free(depth);
    free(gpu_pixels);
    free(gpu_depth);
    ws_framebuffer_free(&fb);
    ws_framebuffer_free(&fb2);
    ws_opencl_destroy(cl);
    return 1;
}

/* --- tile bin (host-only, runs with no OpenCL device) -------------------- */
/* For every pixel, the tile it falls in must list every triangle whose screen
 * bounding box contains that pixel. If the bin missed one, the tiled kernel
 * could drop a covering triangle and stop being bit-identical. This checks the
 * bin directly on a deterministic batch. */
static int test_tilebin_covers_bounding_boxes(void)
{
    enum { W = 200, H = 140, TRI = 400, TS = 16 };
    static float vx[TRI * 3], vy[TRI * 3];
    ws_rng rng;
    ws_rng_init(&rng, 0xC0FFEEULL, 11ULL);
    for (uint32_t t = 0; t < TRI; ++t) {
        float cx = (float)((uint32_t)ws_rng_next_u64(&rng) % W);
        float cy = (float)((uint32_t)ws_rng_next_u64(&rng) % H);
        for (int k = 0; k < 3; ++k) {
            vx[t * 3u + (uint32_t)k] = cx + (float)((int)((uint32_t)ws_rng_next_u64(&rng) % 61u) - 30);
            vy[t * 3u + (uint32_t)k] = cy + (float)((int)((uint32_t)ws_rng_next_u64(&rng) % 61u) - 30);
        }
    }

    ws_tilebin bin;
    WS_CHECK(ws_tilebin_build(&bin, vx, vy, TRI, W, H, TS) == WS_OK);
    WS_CHECK(bin.tiles_x == (W + TS - 1) / TS);
    WS_CHECK(bin.tiles_y == (H + TS - 1) / TS);

    int ok = 1;
    for (uint32_t t = 0; t < TRI && ok; ++t) {
        float ax = vx[t * 3u + 0u], ay = vy[t * 3u + 0u];
        float bx = vx[t * 3u + 1u], by = vy[t * 3u + 1u];
        float cx = vx[t * 3u + 2u], cy = vy[t * 3u + 2u];
        float min_x = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
        float max_x = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
        float min_y = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
        float max_y = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
        if (min_x > (float)(W - 1) || max_x < 0.0f || min_y > (float)(H - 1) || max_y < 0.0f) {
            continue;
        }
        int x0 = min_x < 0.0f ? 0 : (int)min_x;
        int x1 = max_x > (float)(W - 1) ? W - 1 : (int)max_x;
        int y0 = min_y < 0.0f ? 0 : (int)min_y;
        int y1 = max_y > (float)(H - 1) ? H - 1 : (int)max_y;
        for (int y = y0; y <= y1 && ok; ++y) {
            for (int x = x0; x <= x1 && ok; ++x) {
                uint32_t tile = (uint32_t)(y / TS) * bin.tiles_x + (uint32_t)(x / TS);
                int found = 0;
                for (uint32_t li = bin.tile_offset[tile]; li < bin.tile_offset[tile + 1u]; ++li) {
                    if (bin.tri_index[li] == t) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    ok = 0;
                }
            }
        }
    }
    WS_CHECK(ok == 1);
    WS_CHECK(bin.total_refs > 0u);
    ws_tilebin_free(&bin);
    return 1;
}

int main(void)
{
    const ws_test_case cases[] = {
        {"scheduler_determinism", test_scheduler_determinism},
        {"scheduler_empty_range", test_scheduler_empty_range},
        {"hardware_probe", test_hardware_probe},
        {"tilebin_covers_bboxes", test_tilebin_covers_bounding_boxes},
        {"gpu_semantics_and_loss", test_gpu_semantics_and_loss},
    };
    return ws_test_run("gpu", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
