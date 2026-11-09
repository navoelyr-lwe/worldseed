/* Worldseed Engine — headless benchmark.
 *
 * Measures P1 core throughput (world steps, RNG draws, entity create/destroy,
 * event pushes) and CPU raster throughput. Emits machine-readable lines so CI
 * can store performance evidence alongside the smoke output. This is a
 * baseline, not a release performance gate.
 */
#include "core/ws_hash.h"
#include "core/ws_rng.h"
#include "core/ws_result.h"
#include "platform/ws_clock.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"
#include "world/ws_world.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BENCH_ENTITY_CAPACITY 4096u
#define BENCH_EVENT_CAPACITY 4096u

static double bench_world_steps(ws_world *world, uint64_t steps)
{
    uint64_t t0 = ws_clock_monotonic_ns();
    (void)ws_world_advance(world, steps);
    uint64_t t1 = ws_clock_monotonic_ns();
    return ws_clock_ns_to_seconds(t1 - t0);
}

static void bench_entities(ws_world *world)
{
    ws_vec3 p = ws_vec3_make(0.0, 0.0, 0.0);
    uint64_t t0 = ws_clock_monotonic_ns();
    ws_handle handles[BENCH_ENTITY_CAPACITY];
    uint32_t made = 0;
    for (uint32_t i = 0; i < BENCH_ENTITY_CAPACITY; ++i) {
        handles[i] = ws_entity_create(&world->entities, i, p);
        if (ws_handle_is_valid(handles[i])) {
            made += 1u;
        }
    }
    for (uint32_t i = 0; i < made; ++i) {
        (void)ws_entity_destroy(&world->entities, handles[i]);
    }
    uint64_t t1 = ws_clock_monotonic_ns();
    printf("bench entities created=%u seconds=%.6f\n", made,
           ws_clock_ns_to_seconds(t1 - t0));
}

static void bench_events(ws_world *world)
{
    uint64_t t0 = ws_clock_monotonic_ns();
    const uint32_t n = 100000u;
    for (uint32_t i = 0; i < n; ++i) {
        (void)ws_event_log_push(&world->events, WS_EVENT_WORLD_TICK, i, NULL, 0u);
    }
    uint64_t t1 = ws_clock_monotonic_ns();
    printf("bench events pushed=%u seconds=%.6f\n", n, ws_clock_ns_to_seconds(t1 - t0));
}

static void bench_rng(void)
{
    ws_rng rng;
    ws_rng_init(&rng, 1234ULL, 1ULL);
    const uint64_t n = 10000000ULL;
    uint64_t t0 = ws_clock_monotonic_ns();
    uint64_t sink = 0u;
    for (uint64_t i = 0; i < n; ++i) {
        sink ^= ws_rng_next_u64(&rng);
    }
    uint64_t t1 = ws_clock_monotonic_ns();
    double seconds = ws_clock_ns_to_seconds(t1 - t0);
    printf("bench rng draws=%llu seconds=%.6f draws_per_second=%.0f sink=%llu\n",
           (unsigned long long)n, seconds, (double)n / seconds,
           (unsigned long long)sink);
}

static void bench_raster(uint32_t width, uint32_t height, uint32_t tile, int iterations)
{
    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, width, height) != WS_OK) {
        return;
    }
    size_t count = (size_t)width * height;
    float *depth = (float *)malloc(count * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        return;
    }
    const float vx[3] = {0.0f, (float)(width - 1u), (float)(width / 2u)};
    const float vy[3] = {(float)(height - 1u), (float)(height - 1u), 0.0f};
    const float vz[3] = {0.3f, 0.3f, 0.3f};
    const unsigned char color[9] = {200u, 120u, 60u, 60u, 200u, 120u, 120u, 60u, 200u};

    uint64_t t0 = ws_clock_monotonic_ns();
    for (int i = 0; i < iterations; ++i) {
        for (size_t d = 0; d < count; ++d) {
            depth[d] = 1.0e30f;
        }
        ws_framebuffer_clear(&fb, 0u, 0u, 0u, 255u);
        ws_raster_triangle(&fb, depth, vx, vy, vz, color, tile, NULL);
    }
    uint64_t t1 = ws_clock_monotonic_ns();
    double seconds = ws_clock_ns_to_seconds(t1 - t0);
    double frames = (double)iterations / seconds;
    printf("bench raster %ux%u tile=%u iterations=%d seconds=%.6f fps=%.1f\n",
           width, height, tile, iterations, seconds, frames);

    free(depth);
    ws_framebuffer_free(&fb);
}

int main(void)
{
    static WS_ARENA_BLOCK_ALIGN unsigned char entity_arena[BENCH_ENTITY_CAPACITY * 128u];
    static ws_event event_storage[BENCH_EVENT_CAPACITY];

    ws_world_config config;
    config.arena_block = entity_arena;
    config.arena_size = sizeof(entity_arena);
    config.entity_capacity = BENCH_ENTITY_CAPACITY;
    config.event_storage = event_storage;
    config.event_capacity = BENCH_EVENT_CAPACITY;

    ws_world world;
    if (ws_world_init(&world, &config, 1ULL, 20260101ULL) != WS_OK) {
        fprintf(stderr, "bench world init failed\n");
        return 1;
    }

    double t = bench_world_steps(&world, 1000000ULL);
    printf("bench world_steps=1000000 seconds=%.6f steps_per_second=%.0f\n",
           t, 1000000.0 / t);

    bench_entities(&world);
    bench_events(&world);
    bench_rng();
    bench_raster(640u, 480u, 32u, 200);
    bench_raster(1920u, 1080u, 32u, 30);

    printf("WS_BENCH_OK\n");
    return 0;
}
