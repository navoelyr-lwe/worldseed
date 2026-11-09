/* Worldseed Engine — world context and renderer unit tests. */
#include "ws_test.h"

#include "core/ws_hash.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"
#include "world/ws_world.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static int test_world_init_and_step(void)
{
    ws_world world;
    WS_CHECK(ws_world_init(&world, 7ULL, 4242ULL) == WS_OK);
    WS_CHECK(world.world_uid == 7ULL);
    WS_CHECK(world.seed == 4242ULL);
    WS_CHECK(world.tick == 0ULL);
    WS_CHECK(world.schema_version == WS_WORLD_SCHEMA_VERSION);

    WS_CHECK(ws_world_advance(&world, 50ULL) == WS_OK);
    WS_CHECK(world.tick == 50ULL);
    WS_CHECK(fabs(world.time_seconds - 50.0 * WS_WORLD_FIXED_DT) < 1e-9);
    return 1;
}

static int test_world_determinism(void)
{
    ws_world a;
    ws_world b;
    WS_CHECK(ws_world_init(&a, 1ULL, 999ULL) == WS_OK);
    WS_CHECK(ws_world_init(&b, 1ULL, 999ULL) == WS_OK);
    WS_CHECK(ws_world_advance(&a, 200ULL) == WS_OK);
    WS_CHECK(ws_world_advance(&b, 200ULL) == WS_OK);
    WS_CHECK(a.tick == b.tick);
    WS_CHECK(a.time_seconds == b.time_seconds);
    return 1;
}

static int test_world_null(void)
{
    WS_CHECK(ws_world_init(NULL, 1ULL, 1ULL) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_world_step(NULL) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_world_advance(NULL, 1ULL) == WS_ERR_NULL_POINTER);
    return 1;
}

static int test_framebuffer(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 16u, 8u) == WS_OK);
    WS_CHECK(fb.pixels != NULL);
    ws_framebuffer_clear(&fb, 10u, 20u, 30u, 255u);
    uint8_t *p = &fb.pixels[0];
    WS_CHECK(p[0] == 10u && p[1] == 20u && p[2] == 30u && p[3] == 255u);

    ws_framebuffer_set(&fb, 5u, 5u, 1u, 2u, 3u, 4u);
    uint8_t *q = &fb.pixels[((size_t)5 * fb.width + 5u) * 4u];
    WS_CHECK(q[0] == 1u && q[1] == 2u && q[2] == 3u && q[3] == 4u);

    /* Out-of-range writes are ignored, not corrupting memory. */
    ws_framebuffer_set(&fb, 999u, 999u, 9u, 9u, 9u, 9u);

    ws_framebuffer_free(&fb);
    WS_CHECK(fb.pixels == NULL);
    WS_CHECK(fb.width == 0u && fb.height == 0u);
    return 1;
}

static int test_framebuffer_bad_args(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 0u, 10u) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_framebuffer_create(&fb, 10u, 0u) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_framebuffer_create(NULL, 10u, 10u) == WS_ERR_NULL_POINTER);
    return 1;
}

static int test_raster_draws_expected_pixels(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 64u, 64u) == WS_OK);
    ws_framebuffer_clear(&fb, 0u, 0u, 0u, 255u);

    size_t count = (size_t)fb.width * fb.height;
    float *depth = (float *)malloc(count * sizeof(float));
    WS_CHECK(depth != NULL);
    for (size_t i = 0; i < count; ++i) {
        depth[i] = 1.0e30f;
    }

    const float vx[3] = {0.0f, 63.0f, 31.0f};
    const float vy[3] = {63.0f, 63.0f, 0.0f};
    const float vz[3] = {0.1f, 0.1f, 0.1f};
    const unsigned char color[3][3] = {{255u, 0u, 0u}, {0u, 255u, 0u}, {0u, 0u, 255u}};

    ws_raster_tile_stats stats;
    memset(&stats, 0, sizeof(stats));
    ws_raster_triangle(&fb, depth, vx, vy, vz, color, 8u, &stats);

    WS_CHECK(stats.triangles_drawn == 1u);
    WS_CHECK(stats.tiles_visited > 0u);
    WS_CHECK(stats.pixels_shaded > 0u);

    /* A pixel inside the triangle must have been shaded. */
    uint8_t *center = &fb.pixels[((size_t)40 * fb.width + 31u) * 4u];
    WS_CHECK(center[3] == 255u);
    WS_CHECK(center[0] != 0u || center[1] != 0u || center[2] != 0u);

    free(depth);
    ws_framebuffer_free(&fb);
    return 1;
}

static int test_raster_depth_rejects_farther(void)
{
    ws_framebuffer fb;
    WS_CHECK(ws_framebuffer_create(&fb, 32u, 32u) == WS_OK);
    ws_framebuffer_clear(&fb, 0u, 0u, 0u, 255u);

    size_t count = (size_t)fb.width * fb.height;
    float *depth = (float *)malloc(count * sizeof(float));
    WS_CHECK(depth != NULL);
    for (size_t i = 0; i < count; ++i) {
        depth[i] = 1.0e30f;
    }

    const float vx[3] = {0.0f, 31.0f, 15.0f};
    const float vy[3] = {31.0f, 31.0f, 0.0f};
    const unsigned char near_color[3][3] = {{255u, 255u, 255u}, {255u, 255u, 255u}, {255u, 255u, 255u}};
    const unsigned char far_color[3][3] = {{1u, 1u, 1u}, {1u, 1u, 1u}, {1u, 1u, 1u}};
    const float near_z[3] = {0.1f, 0.1f, 0.1f};
    const float far_z[3] = {0.9f, 0.9f, 0.9f};

    ws_raster_triangle(&fb, depth, vx, vy, near_z, near_color, 0u, NULL);
    ws_raster_triangle(&fb, depth, vx, vy, far_z, far_color, 0u, NULL);

    uint8_t *center = &fb.pixels[((size_t)20 * fb.width + 15u) * 4u];
    WS_CHECK(center[0] == 255u && center[1] == 255u && center[2] == 255u);

    free(depth);
    ws_framebuffer_free(&fb);
    return 1;
}

static int test_raster_determinism(void)
{
    const float vx[3] = {2.0f, 30.0f, 16.0f};
    const float vy[3] = {28.0f, 28.0f, 2.0f};
    const float vz[3] = {0.3f, 0.3f, 0.3f};
    const unsigned char color[3][3] = {{200u, 100u, 50u}, {50u, 200u, 100u}, {100u, 50u, 200u}};

    ws_framebuffer a;
    ws_framebuffer b;
    WS_CHECK(ws_framebuffer_create(&a, 32u, 32u) == WS_OK);
    WS_CHECK(ws_framebuffer_create(&b, 32u, 32u) == WS_OK);
    ws_framebuffer_clear(&a, 0u, 0u, 0u, 255u);
    ws_framebuffer_clear(&b, 0u, 0u, 0u, 255u);

    ws_raster_triangle(&a, NULL, vx, vy, vz, color, 16u, NULL);
    ws_raster_triangle(&b, NULL, vx, vy, vz, color, 8u, NULL);

    size_t bytes = (size_t)a.width * a.height * 4u;
    WS_CHECK(memcmp(a.pixels, b.pixels, bytes) == 0);

    ws_framebuffer_free(&a);
    ws_framebuffer_free(&b);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"world init and step", test_world_init_and_step},
        {"world determinism", test_world_determinism},
        {"world null safety", test_world_null},
        {"framebuffer basics", test_framebuffer},
        {"framebuffer bad args", test_framebuffer_bad_args},
        {"raster draws pixels", test_raster_draws_expected_pixels},
        {"raster depth test", test_raster_depth_rejects_farther},
        {"raster tile-size determinism", test_raster_determinism},
    };
    return ws_test_run("world_render", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
