/* Worldseed Engine — performance and hardware evidence tool (P10).
 *
 * Emits machine-readable lines for CI evidence and the release benchmark report:
 *   - the detected hardware profile (CPU threads, RAM, OS, OpenCL device);
 *   - CPU vs GPU raster time on the same deterministic triangle batch, and the
 *     speed-up, so "GPU only when it actually helps" is measured, not assumed;
 *   - multi-core scheduler scaling (serial vs parallel over a pure workload)
 *     with an identical-result check, so parallelism never changes the data;
 *   - 4K reference-scene frame times with p50/p95/p99 (the release metric that
 *     one short FPS sample cannot stand in for, docs/11).
 *
 * Ends with WS_PERF_OK. It runs with or without a GPU device.
 */
#include "core/ws_rng.h"
#include "core/ws_sched.h"
#include "gpu/ws_opencl.h"
#include "platform/ws_clock.h"
#include "platform/ws_hardware.h"
#include "render/ws_catalog.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"
#include "render/ws_render.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PERF_TRIANGLES 2048u
#define PERF_RASTER_WIDTH 1920u
#define PERF_RASTER_HEIGHT 1080u
#define PERF_SCENE_WIDTH 3840u
#define PERF_SCENE_HEIGHT 2160u
#define PERF_SCENE_FRAMES 24
#define PERF_QUICK_WIDTH 1920u
#define PERF_QUICK_HEIGHT 1080u
#define PERF_QUICK_FRAMES 4
#define PERF_TILE 32u

static double ns_to_ms(uint64_t ns)
{
    return (double)ns / 1.0e6;
}

static int cmp_double(const void *a, const void *b)
{
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) { return -1; }
    if (da > db) { return 1; }
    return 0;
}

static double percentile(const double *sorted, size_t n, double p)
{
    if (n == 0u) {
        return 0.0;
    }
    double idx = p * (double)(n - 1u);
    size_t lo = (size_t)idx;
    size_t hi = lo + 1u < n ? lo + 1u : lo;
    double frac = idx - (double)lo;
    return sorted[lo] * (1.0 - frac) + sorted[hi] * frac;
}

/* Small deterministic triangle batch (synthetic screen-space geometry). */
static void build_batch(float *vx, float *vy, float *vz, uint8_t *color, uint32_t count,
                        uint32_t width, uint32_t height)
{
    ws_rng rng;
    ws_rng_init(&rng, 0xC0FFEEULL, 11ULL);
    vx[0] = -1.0f; vy[0] = -1.0f; vz[0] = 0.95f;
    vx[1] = (float)width + 1.0f; vy[1] = -1.0f; vz[1] = 0.95f;
    vx[2] = (float)width * 0.5f; vy[2] = (float)height + 1.0f; vz[2] = 0.95f;
    color[0] = 25u; color[1] = 35u; color[2] = 55u;
    color[3] = 55u; color[4] = 45u; color[5] = 25u;
    color[6] = 15u; color[7] = 55u; color[8] = 35u;

    const float half = 26.0f;
    for (uint32_t t = 1u; t < count; ++t) {
        uint32_t b = t * 3u;
        uint32_t c = t * 9u;
        float cx = (float)((uint32_t)ws_rng_next_u64(&rng) % width);
        float cy = (float)((uint32_t)ws_rng_next_u64(&rng) % height);
        for (int k = 0; k < 3; ++k) {
            double ang = (double)((uint32_t)ws_rng_next_u64(&rng) % 628u) / 100.0;
            double rad = half * (double)((uint32_t)ws_rng_next_u64(&rng) % 1000u) / 1000.0 + 4.0;
            vx[b + (uint32_t)k] = cx + (float)(rad * cos(ang));
            vy[b + (uint32_t)k] = cy + (float)(rad * sin(ang));
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
    size_t n = (size_t)fb->width * fb->height;
    for (size_t i = 0; i < n; ++i) {
        depth[i] = 1.0e30f;
    }
    ws_framebuffer_clear(fb, 0u, 0u, 0u, 255u);
    for (uint32_t t = 0u; t < count; ++t) {
        ws_raster_triangle(fb, depth, &vx[t * 3u], &vy[t * 3u], &vz[t * 3u],
                           &color[t * 9u], PERF_TILE, NULL);
    }
}

/* --- scheduler scaling ---------------------------------------------------- */
typedef struct scale_ctx {
    const float *vx;
    const float *vy;
    const float *vz;
    uint32_t count;
    uint64_t *sink;
} scale_ctx;

/* A pure, index-addressed workload: a short numerical integration per index.
 * Writing to sink[index] is a private slot, so parallel and serial results are
 * identical. The work per index is deliberately large enough that the pool's
 * per-job synchronisation is amortised, so the reported speed-up reflects the
 * cores rather than wake-up latency. */
static void scale_body(void *ctxp, uint32_t index)
{
    scale_ctx *ctx = (scale_ctx *)ctxp;
    const float *tx = &ctx->vx[(size_t)index * 3u];
    const float *ty = &ctx->vy[(size_t)index * 3u];
    double area = (tx[2] - tx[0]) * (ty[1] - ty[0]) - (ty[2] - ty[0]) * (tx[1] - tx[0]);
    double acc = fabs(area);
    for (int k = 0; k < 4000; ++k) {
        acc = acc * 1.0000001 + (double)(k & 7) * 0.5;
        acc = acc - (double)(k & 3) * 0.25;
    }
    ctx->sink[index] = (uint64_t)acc;
}

static void run_scaling(const float *vx, const float *vy, const float *vz)
{
    static uint64_t serial_sink[PERF_TRIANGLES];
    static uint64_t parallel_sink[PERF_TRIANGLES];
    scale_ctx ctx;
    ctx.vx = vx; ctx.vy = vy; ctx.vz = vz; ctx.count = PERF_TRIANGLES;

    /* Repeat to get a measurable duration. */
    const int reps = 8;
    uint64_t t0 = ws_clock_monotonic_ns();
    for (int r = 0; r < reps; ++r) {
        ctx.sink = serial_sink;
        for (uint32_t i = 0u; i < PERF_TRIANGLES; ++i) {
            scale_body(&ctx, i);
        }
    }
    uint64_t t1 = ws_clock_monotonic_ns();

    ws_sched *sched = NULL;
    if (ws_sched_create_default(&sched) != WS_OK) {
        printf("perf sched=unavailable\n");
        return;
    }
    uint64_t t2 = ws_clock_monotonic_ns();
    for (int r = 0; r < reps; ++r) {
        ctx.sink = parallel_sink;
        ws_sched_parallel_for(sched, 0u, PERF_TRIANGLES, scale_body, &ctx);
    }
    uint64_t t3 = ws_clock_monotonic_ns();

    int identical = memcmp(serial_sink, parallel_sink, sizeof(serial_sink)) == 0;
    double serial_ms = ns_to_ms(t1 - t0);
    double parallel_ms = ns_to_ms(t3 - t2);
    double speedup = parallel_ms > 0.0 ? serial_ms / parallel_ms : 0.0;
    printf("perf scheduler workers=%u serial_ms=%.3f parallel_ms=%.3f speedup=%.2fx "
           "identical=%d\n",
           ws_sched_worker_count(sched), serial_ms, parallel_ms, speedup, identical);
    if (!identical) {
        printf("perf scheduler WARNING: parallel result differs from serial\n");
    }
    ws_sched_destroy(sched);
}

/* --- reference scene frame times ------------------------------------------ */
static void run_scene_frames(uint64_t seed, uint32_t width, uint32_t height, int frames)
{
    ws_catalog catalog;
    memset(&catalog, 0, sizeof(catalog));
    size_t showcase_count = ws_showcase_scene_count();
    size_t day_index = showcase_count; /* first environment scene = day */
    if (ws_catalog_build(&catalog, day_index, seed) != WS_OK) {
        printf("perf scene=unavailable\n");
        return;
    }
    const ws_scene *scene = ws_catalog_scene(&catalog);

    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, width, height) != WS_OK) {
        ws_catalog_free(&catalog);
        printf("perf scene=alloc_failed\n");
        return;
    }
    float *depth = (float *)malloc((size_t)width * height * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        ws_catalog_free(&catalog);
        printf("perf scene=alloc_failed\n");
        return;
    }

    double times[64];
    if (frames > 64) { frames = 64; }
    if (frames < 1) { frames = 1; }
    ws_render_stats stats;
    memset(&stats, 0, sizeof(stats));
    /* One warm-up frame, then measured frames. */
    (void)ws_render_scene(scene, &fb, depth, PERF_TILE, &stats);
    for (int i = 0; i < frames; ++i) {
        uint64_t t0 = ws_clock_monotonic_ns();
        (void)ws_render_scene(scene, &fb, depth, PERF_TILE, &stats);
        uint64_t t1 = ws_clock_monotonic_ns();
        times[i] = ns_to_ms(t1 - t0);
    }
    double sorted[64];
    memcpy(sorted, times, (size_t)frames * sizeof(double));
    qsort(sorted, (size_t)frames, sizeof(double), cmp_double);
    double p50 = percentile(sorted, (size_t)frames, 0.50);
    double p95 = percentile(sorted, (size_t)frames, 0.95);
    double p99 = percentile(sorted, (size_t)frames, 0.99);
    double avg = 0.0;
    for (int i = 0; i < frames; ++i) {
        avg += times[i];
    }
    avg /= (double)frames;
    printf("perf scene scene=%s %ux%u frames=%d avg_ms=%.2f p50_ms=%.2f p95_ms=%.2f "
           "p99_ms=%.2f avg_fps=%.1f triangles=%llu\n",
           ws_catalog_id(day_index), width, height, frames, avg, p50, p95, p99,
           1000.0 / avg, (unsigned long long)stats.triangles_submitted);

    free(depth);
    ws_framebuffer_free(&fb);
    ws_catalog_free(&catalog);
}

int main(int argc, char **argv)
{
    int quick = 0;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--quick") == 0) {
            quick = 1;
        }
    }

    ws_hardware_info hw;
    (void)ws_hardware_probe(&hw);
    char desc[512];
    ws_hardware_describe(&hw, desc, sizeof(desc));
    printf("perf mode=%s\n", quick ? "quick" : "full");
    printf("perf hardware %s\n", desc);

    static float vx[PERF_TRIANGLES * 3u];
    static float vy[PERF_TRIANGLES * 3u];
    static float vz[PERF_TRIANGLES * 3u];
    static uint8_t color[PERF_TRIANGLES * 9u];
    build_batch(vx, vy, vz, color, PERF_TRIANGLES, PERF_RASTER_WIDTH, PERF_RASTER_HEIGHT);

    run_scaling(vx, vy, vz);

    uint32_t rw = quick ? PERF_QUICK_WIDTH : PERF_RASTER_WIDTH;
    uint32_t rh = quick ? PERF_QUICK_HEIGHT : PERF_RASTER_HEIGHT;

    /* CPU raster timing for the batch. */
    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, rw, rh) != WS_OK) {
        fprintf(stderr, "framebuffer create failed\n");
        return 1;
    }
    size_t pixel_count = (size_t)rw * rh;
    float *depth = (float *)malloc(pixel_count * sizeof(float));
    uint8_t *gpu_pixels = (uint8_t *)malloc(pixel_count * 4u);
    float *gpu_depth = (float *)malloc(pixel_count * sizeof(float));
    if (depth == NULL || gpu_pixels == NULL || gpu_depth == NULL) {
        fprintf(stderr, "allocation failed\n");
        return 1;
    }

    uint64_t c0 = ws_clock_monotonic_ns();
    raster_cpu(vx, vy, vz, color, PERF_TRIANGLES, &fb, depth);
    uint64_t c1 = ws_clock_monotonic_ns();
    double cpu_ms = ns_to_ms(c1 - c0);
    printf("perf raster_cpu %ux%u triangles=%u ms=%.2f\n", rw, rh, PERF_TRIANGLES, cpu_ms);

    ws_opencl *cl = NULL;
    if (ws_opencl_create(&cl) == WS_OK && ws_opencl_available(cl)) {
        const ws_opencl_info *info = ws_opencl_info_of(cl);
        ws_result rr = ws_opencl_raster(cl, vx, vy, vz, color, PERF_TRIANGLES,
                                        rw, rh, gpu_pixels, gpu_depth);
        if (rr == WS_OK) {
            uint64_t g0 = ws_clock_monotonic_ns();
            rr = ws_opencl_raster(cl, vx, vy, vz, color, PERF_TRIANGLES,
                                  rw, rh, gpu_pixels, gpu_depth);
            uint64_t g1 = ws_clock_monotonic_ns();
            double gpu_ms = ns_to_ms(g1 - g0);
            uint64_t diff = 0u;
            for (size_t i = 0; i < pixel_count; ++i) {
                if (fb.pixels[i * 4u + 0u] != gpu_pixels[i * 4u + 0u] ||
                    fb.pixels[i * 4u + 1u] != gpu_pixels[i * 4u + 1u] ||
                    fb.pixels[i * 4u + 2u] != gpu_pixels[i * 4u + 2u]) {
                    diff += 1u;
                }
            }
            double frac = (double)diff / (double)pixel_count;
            double speedup = gpu_ms > 0.0 ? cpu_ms / gpu_ms : 0.0;
            const char *verdict = speedup >= 1.0 ? "use_gpu" : "use_cpu";
            printf("perf raster_gpu %ux%u device=\"%s\" ms=%.2f speedup_vs_cpu=%.2fx "
                   "diff_fraction=%.6f verdict=%s\n",
                   rw, rh, info->device_name, gpu_ms, speedup, frac, verdict);
        } else {
            printf("perf raster_gpu unavailable_after_create\n");
        }
    } else {
        printf("perf raster_gpu gpu=unavailable\n");
    }
    ws_opencl_destroy(cl);

    free(depth);
    free(gpu_pixels);
    free(gpu_depth);
    ws_framebuffer_free(&fb);

    if (quick) {
        run_scene_frames(20260101ULL, PERF_QUICK_WIDTH, PERF_QUICK_HEIGHT, PERF_QUICK_FRAMES);
    } else {
        run_scene_frames(20260101ULL, PERF_SCENE_WIDTH, PERF_SCENE_HEIGHT, PERF_SCENE_FRAMES);
    }

    printf("WS_PERF_OK\n");
    return 0;
}
