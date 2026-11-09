/* Worldseed Engine — headless smoke tool.
 *
 * Proves the core builds and runs headless: creates a world, advances it,
 * renders a reference tile, probes OpenCL and writes a PPM frame. Emits a small
 * machine-readable summary so CI can capture evidence without a display.
 */
#include "core/ws_hash.h"
#include "core/ws_rng.h"
#include "core/ws_result.h"
#include "platform/ws_opencl_probe.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"
#include "world/ws_world.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SMOKE_ENTITY_CAPACITY 256u
#define SMOKE_EVENT_CAPACITY 512u

static int render_reference_frame(const char *out_path, char *digest_hex)
{
    ws_framebuffer fb;
    if (ws_framebuffer_create(&fb, 320u, 240u) != WS_OK) {
        return 1;
    }
    ws_framebuffer_clear(&fb, 20u, 24u, 34u, 255u);

    size_t depth_count = (size_t)fb.width * (size_t)fb.height;
    float *depth = (float *)malloc(depth_count * sizeof(float));
    if (depth == NULL) {
        ws_framebuffer_free(&fb);
        return 1;
    }
    for (size_t i = 0; i < depth_count; ++i) {
        depth[i] = 1.0e30f;
    }

    const float vx[3] = {40.0f, 280.0f, 160.0f};
    const float vy[3] = {200.0f, 200.0f, 40.0f};
    const float vz[3] = {0.5f, 0.5f, 0.2f};
    const unsigned char color[3][3] = {{90u, 160u, 70u}, {70u, 140u, 60u}, {120u, 180u, 90u}};

    ws_raster_tile_stats stats;
    memset(&stats, 0, sizeof(stats));
    ws_raster_triangle(&fb, depth, vx, vy, vz, color, 32u, &stats);

    printf("raster tiles_visited=%llu triangles=%llu pixels=%llu\n",
           (unsigned long long)stats.tiles_visited,
           (unsigned long long)stats.triangles_drawn,
           (unsigned long long)stats.pixels_shaded);

    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_buffer(fb.pixels, (size_t)fb.width * fb.height * 4u, digest);
    ws_sha256_hex(digest, digest_hex);

    ws_result wr = ws_framebuffer_write_ppm(&fb, out_path);
    free(depth);
    ws_framebuffer_free(&fb);
    return wr == WS_OK ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *frame_path = (argc > 1) ? argv[1] : "smoke_frame.ppm";
    uint64_t seed = (argc > 2) ? strtoull(argv[2], NULL, 10) : 20260101ULL;

    static WS_ARENA_BLOCK_ALIGN unsigned char entity_arena[SMOKE_ENTITY_CAPACITY * 128u];
    static ws_event event_storage[SMOKE_EVENT_CAPACITY];

    ws_world_config config;
    config.arena_block = entity_arena;
    config.arena_size = sizeof(entity_arena);
    config.entity_capacity = SMOKE_ENTITY_CAPACITY;
    config.event_storage = event_storage;
    config.event_capacity = SMOKE_EVENT_CAPACITY;

    ws_world world;
    if (ws_world_init(&world, &config, 1ULL, seed) != WS_OK) {
        fprintf(stderr, "world init failed\n");
        return 1;
    }
    if (ws_world_advance(&world, 1000ULL) != WS_OK) {
        fprintf(stderr, "world advance failed\n");
        return 1;
    }

    ws_rng rng;
    ws_rng_init(&rng, seed, 7ULL);
    double sample = ws_rng_next_double(&rng);

    ws_opencl_info cl;
    ws_opencl_probe(&cl);

    char digest_hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    if (render_reference_frame(frame_path, digest_hex) != 0) {
        fprintf(stderr, "render failed\n");
        return 1;
    }

    printf("world uid=%llu seed=%llu tick=%llu time=%.3f events=%u\n",
           (unsigned long long)world.world_uid,
           (unsigned long long)world.seed,
           (unsigned long long)world.tick,
           world.time_seconds,
           ws_event_log_count(&world.events));
    printf("rng sample=%.6f\n", sample);
    printf("opencl loaded=%d device_found=%d device=\"%s\" version=\"%s\" units=%u mem=%llu\n",
           cl.runtime_loaded, cl.device_found, cl.device_name, cl.device_version,
           cl.compute_units, (unsigned long long)cl.global_memory_bytes);
    printf("frame path=%s sha256=%s\n", frame_path, digest_hex);
    printf("WS_SMOKE_OK\n");
    return 0;
}
