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
#include "render/ws_mesh.h"
#include "render/ws_raster.h"
#include "render/ws_render.h"
#include "render/ws_scene.h"
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

/* Render a small lit scene (plane + sphere + cube) at several resolutions and
 * report frames per second for the reference CPU renderer. */
static void bench_scene_render(uint32_t width, uint32_t height, int iterations)
{
    static ws_vertex cube_v[24];
    static uint32_t cube_i[36];
    static ws_vertex plane_v[4];
    static uint32_t plane_i[6];
    static ws_vertex sphere_v[17u * 9u];
    static uint32_t sphere_i[16u * 9u * 6u];
    const uint8_t cc[3] = {180u, 150u, 90u};
    const uint8_t pc[3] = {70u, 110u, 60u};
    const uint8_t sc[3] = {120u, 90u, 70u};
    ws_mesh cube, plane, sphere;
    if (ws_mesh_make_cube(&cube, cube_v, 24u, cube_i, 36u, cc) != WS_OK ||
        ws_mesh_make_plane(&plane, plane_v, 4u, plane_i, 6u, 8.0, 1u, pc) != WS_OK ||
        ws_mesh_make_sphere(&sphere, sphere_v, 17u * 9u, sphere_i, 16u * 9u * 6u, 16u, 8u, sc) != WS_OK) {
        return;
    }
    ws_scene_object objects[4];
    ws_scene scene;
    if (ws_scene_init(&scene, objects, 4u) != WS_OK) {
        return;
    }
    ws_material ground = {{0.30, 0.42, 0.24}, 0.8, 0.0};
    ws_material body = {{0.72, 0.58, 0.36}, 0.8, 0.0};
    (void)ws_scene_add_object(&scene, &plane, ws_mat4_translation(ws_vec3_make(0.0, -0.5, 0.0)), ground);
    (void)ws_scene_add_object(&scene, &cube, ws_mat4_identity(), body);
    (void)ws_scene_add_object(&scene, &sphere,
                              ws_mat4_multiply(ws_mat4_translation(ws_vec3_make(1.4, 0.0, 0.6)),
                                               ws_mat4_scale(ws_vec3_make(0.8, 0.8, 0.8))), body);
    scene.camera = ws_camera_default();

    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, width, height) != WS_OK) {
        return;
    }
    size_t count = (size_t)width * height;
    float *depth = malloc(count * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        return;
    }
    ws_render_stats stats;
    uint64_t t0 = ws_clock_monotonic_ns();
    for (int i = 0; i < iterations; ++i) {
        (void)ws_render_scene(&scene, &fb, depth, 32u, &stats);
    }
    uint64_t t1 = ws_clock_monotonic_ns();
    double seconds = ws_clock_ns_to_seconds(t1 - t0);
    printf("bench scene %ux%u iterations=%d seconds=%.6f fps=%.1f triangles=%llu\n",
           width, height, iterations, seconds, (double)iterations / seconds,
           (unsigned long long)stats.triangles_submitted);
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
    bench_scene_render(640u, 480u, 60);
    bench_scene_render(1920u, 1080u, 10);

    printf("WS_BENCH_OK\n");
    return 0;
}
