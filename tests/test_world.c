/* Worldseed Engine — world, entity, event, snapshot and renderer tests. */
#include "ws_test.h"

#include "core/ws_hash.h"
#include "render/ws_framebuffer.h"
#include "render/ws_raster.h"
#include "world/ws_snapshot.h"
#include "world/ws_world.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define TEST_ENTITY_CAPACITY 64u
#define TEST_EVENT_CAPACITY 32u

typedef struct test_world {
    _Alignas(max_align_t) unsigned char arena[TEST_ENTITY_CAPACITY * 128u];
    ws_event events[TEST_EVENT_CAPACITY];
    ws_world world;
} test_world;

static void make_config(ws_world_config *config, test_world *tw)
{
    config->arena_block = tw->arena;
    config->arena_size = sizeof(tw->arena);
    config->entity_capacity = TEST_ENTITY_CAPACITY;
    config->event_storage = tw->events;
    config->event_capacity = TEST_EVENT_CAPACITY;
}

static int test_world_init_and_step(void)
{
    test_world tw;
    ws_world_config config;
    make_config(&config, &tw);
    WS_CHECK(ws_world_init(&tw.world, &config, 7ULL, 4242ULL) == WS_OK);
    WS_CHECK(tw.world.world_uid == 7ULL);
    WS_CHECK(tw.world.seed == 4242ULL);
    WS_CHECK(tw.world.tick == 0ULL);
    WS_CHECK(tw.world.schema_version == WS_WORLD_SCHEMA_VERSION);
    /* world_created event is emitted at init. */
    WS_CHECK(ws_event_log_count(&tw.world.events) == 1u);

    WS_CHECK(ws_world_advance(&tw.world, 50ULL) == WS_OK);
    WS_CHECK(tw.world.tick == 50ULL);
    WS_CHECK(fabs(tw.world.time_seconds - 50.0 * WS_WORLD_FIXED_DT) < 1e-9);
    return 1;
}

static int test_world_determinism(void)
{
    test_world a;
    test_world b;
    ws_world_config ca;
    ws_world_config cb;
    make_config(&ca, &a);
    make_config(&cb, &b);
    WS_CHECK(ws_world_init(&a.world, &ca, 1ULL, 999ULL) == WS_OK);
    WS_CHECK(ws_world_init(&b.world, &cb, 1ULL, 999ULL) == WS_OK);
    WS_CHECK(ws_world_advance(&a.world, 200ULL) == WS_OK);
    WS_CHECK(ws_world_advance(&b.world, 200ULL) == WS_OK);
    WS_CHECK(a.world.tick == b.world.tick);
    WS_CHECK(a.world.time_seconds == b.world.time_seconds);
    WS_CHECK(ws_rng_next_u64(&a.world.rng_world) == ws_rng_next_u64(&b.world.rng_world));
    return 1;
}

static uint64_t rng_fingerprint(uint64_t seed)
{
    ws_rng rng;
    ws_rng_init(&rng, seed, 3ULL);
    uint64_t h = 1469598103934665603ULL; /* FNV-1a offset basis */
    for (int i = 0; i < 64; ++i) {
        h ^= ws_rng_next_u64(&rng);
        h *= 1099511628211ULL;
    }
    return h;
}

static int test_world_multi_seed(void)
{
    /* Distinct seeds must produce distinct streams; each seed must be
     * reproducible across calls. This guards against accidental stream
     * coupling or seed mixing regressions. */
    uint64_t a1 = rng_fingerprint(1ULL);
    uint64_t a2 = rng_fingerprint(1ULL);
    uint64_t b = rng_fingerprint(2ULL);
    uint64_t c = rng_fingerprint(3ULL);
    WS_CHECK(a1 == a2);
    WS_CHECK(a1 != b);
    WS_CHECK(b != c);
    WS_CHECK(a1 != c);
    return 1;
}

static int test_world_replay_identical(void)
{
    /* Two independent worlds with the same seed, advanced the same number of
     * steps, must agree on tick, time and RNG position, and must have identical
     * event logs. This is the P1 replay guarantee. */
    test_world wa;
    test_world wb;
    ws_world_config ca;
    ws_world_config cb;
    make_config(&ca, &wa);
    make_config(&cb, &wb);
    WS_CHECK(ws_world_init(&wa.world, &ca, 100ULL, 20260101ULL) == WS_OK);
    WS_CHECK(ws_world_init(&wb.world, &cb, 100ULL, 20260101ULL) == WS_OK);

    for (int step = 0; step < 500; ++step) {
        WS_CHECK(ws_world_step(&wa.world) == WS_OK);
        WS_CHECK(ws_world_step(&wb.world) == WS_OK);
    }

    WS_CHECK(wa.world.tick == wb.world.tick);
    WS_CHECK(wa.world.time_seconds == wb.world.time_seconds);
    WS_CHECK(ws_rng_next_u64(&wa.world.rng_world) == ws_rng_next_u64(&wb.world.rng_world));

    uint32_t ca_count = ws_event_log_count(&wa.world.events);
    WS_CHECK(ca_count == ws_event_log_count(&wb.world.events));
    for (uint32_t i = 0; i < ca_count; ++i) {
        ws_event ea;
        ws_event eb;
        WS_CHECK(ws_event_log_at(&wa.world.events, i, &ea) == WS_OK);
        WS_CHECK(ws_event_log_at(&wb.world.events, i, &eb) == WS_OK);
        WS_CHECK(ea.sequence == eb.sequence);
        WS_CHECK(ea.type == eb.type);
        WS_CHECK(ea.tick == eb.tick);
    }
    return 1;
}

static int test_world_null(void)
{
    test_world tw;
    ws_world_config config;
    make_config(&config, &tw);
    WS_CHECK(ws_world_init(NULL, &config, 1ULL, 1ULL) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_world_init(&tw.world, NULL, 1ULL, 1ULL) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_world_step(NULL) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_world_advance(NULL, 1ULL) == WS_ERR_NULL_POINTER);
    return 1;
}

static int test_entity_lifecycle(void)
{
    test_world tw;
    ws_world_config config;
    make_config(&config, &tw);
    WS_CHECK(ws_world_init(&tw.world, &config, 1ULL, 5ULL) == WS_OK);
    WS_CHECK(ws_entity_count(&tw.world.entities) == 0u);

    ws_vec3 p = ws_vec3_make(1.0, 2.0, 3.0);
    ws_handle h = ws_entity_create(&tw.world.entities, 42u, p);
    WS_CHECK(ws_handle_is_valid(h));
    WS_CHECK(ws_entity_count(&tw.world.entities) == 1u);

    ws_entity *e = ws_entity_get(&tw.world.entities, h);
    WS_CHECK(e != NULL);
    WS_CHECK(e->kind == 42u);
    WS_CHECK(e->position.x == 1.0 && e->position.y == 2.0 && e->position.z == 3.0);
    WS_CHECK(e->alive == 1);

    WS_CHECK(ws_entity_destroy(&tw.world.entities, h) == WS_OK);
    WS_CHECK(ws_entity_count(&tw.world.entities) == 0u);
    /* Stale handle must not resolve. */
    WS_CHECK(ws_entity_get(&tw.world.entities, h) == NULL);
    WS_CHECK(ws_entity_destroy(&tw.world.entities, h) == WS_ERR_NOT_FOUND);
    return 1;
}

static int test_entity_capacity(void)
{
    test_world tw;
    ws_world_config config;
    make_config(&config, &tw);
    WS_CHECK(ws_world_init(&tw.world, &config, 1ULL, 5ULL) == WS_OK);
    ws_vec3 origin = ws_vec3_make(0.0, 0.0, 0.0);
    for (uint32_t i = 0; i < TEST_ENTITY_CAPACITY; ++i) {
        WS_CHECK(ws_handle_is_valid(ws_entity_create(&tw.world.entities, i, origin)));
    }
    WS_CHECK(!ws_handle_is_valid(ws_entity_create(&tw.world.entities, 0u, origin)));
    WS_CHECK(ws_entity_count(&tw.world.entities) == TEST_ENTITY_CAPACITY);
    return 1;
}

static int test_event_stream_order(void)
{
    ws_event storage[8];
    ws_event_log log;
    WS_CHECK(ws_event_log_init(&log, storage, 8u) == WS_OK);

    uint32_t payload = 0xDEADBEEFu;
    for (uint64_t t = 0; t < 8u; ++t) {
        WS_CHECK(ws_event_log_push(&log, WS_EVENT_WORLD_TICK, t, &payload, sizeof(payload)) == WS_OK);
    }
    WS_CHECK(ws_event_log_count(&log) == 8u);

    uint64_t prev = 0u;
    for (uint32_t i = 0; i < 8u; ++i) {
        ws_event e;
        WS_CHECK(ws_event_log_at(&log, i, &e) == WS_OK);
        WS_CHECK(e.sequence > prev);
        prev = e.sequence;
        WS_CHECK(e.tick == i);
        WS_CHECK(e.payload_size == sizeof(payload));
        WS_CHECK(memcmp(e.payload, &payload, sizeof(payload)) == 0);
    }
    WS_CHECK(ws_event_log_at(&log, 8u, &storage[0]) == WS_ERR_NOT_FOUND);
    return 1;
}

static int test_event_stream_wraparound(void)
{
    ws_event storage[4];
    ws_event_log log;
    WS_CHECK(ws_event_log_init(&log, storage, 4u) == WS_OK);
    for (uint64_t t = 0; t < 6u; ++t) {
        WS_CHECK(ws_event_log_push(&log, WS_EVENT_WORLD_TICK, t, NULL, 0u) == WS_OK);
    }
    /* Capacity is 4, so the oldest 2 events were overwritten. */
    WS_CHECK(ws_event_log_count(&log) == 4u);
    WS_CHECK(ws_event_log_dropped(&log) == 2u);

    ws_event e;
    WS_CHECK(ws_event_log_at(&log, 0u, &e) == WS_OK);
    WS_CHECK(e.tick == 2u); /* oldest retained */
    WS_CHECK(ws_event_log_at(&log, 3u, &e) == WS_OK);
    WS_CHECK(e.tick == 5u); /* newest */
    return 1;
}

static int test_snapshot_readonly_and_incremental(void)
{
    test_world tw;
    ws_world_config config;
    make_config(&config, &tw);
    WS_CHECK(ws_world_init(&tw.world, &config, 9ULL, 11ULL) == WS_OK);

    ws_snapshot snap;
    ws_snapshot_capture(&tw.world, &snap);
    WS_CHECK(snap.world_uid == 9ULL);
    WS_CHECK(snap.seed == 11ULL);
    WS_CHECK(snap.tick == 0ULL);
    WS_CHECK(snap.event_count == 1u); /* world_created */

    /* First read: only world_created so far. */
    ws_event buf[TEST_EVENT_CAPACITY];
    ws_event_log out;
    ws_snapshot_reader reader;
    ws_snapshot_reader_init(&reader);
    WS_CHECK(ws_snapshot_read_events(&tw.world, &reader, &out, buf, TEST_EVENT_CAPACITY) == WS_OK);
    WS_CHECK(ws_event_log_count(&out) == 1u);

    /* Second read without new events: nothing. */
    WS_CHECK(ws_snapshot_read_events(&tw.world, &reader, &out, buf, TEST_EVENT_CAPACITY) == WS_OK);
    WS_CHECK(ws_event_log_count(&out) == 0u);

    /* Emit and read only the new event. */
    WS_CHECK(ws_event_log_push(&tw.world.events, WS_EVENT_ENTITY_CREATED, 1ULL, NULL, 0u) == WS_OK);
    WS_CHECK(ws_snapshot_read_events(&tw.world, &reader, &out, buf, TEST_EVENT_CAPACITY) == WS_OK);
    WS_CHECK(ws_event_log_count(&out) == 1u);
    ws_event e;
    WS_CHECK(ws_event_log_at(&out, 0u, &e) == WS_OK);
    WS_CHECK(e.type == WS_EVENT_ENTITY_CREATED);
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
        {"world multi-seed distinctness", test_world_multi_seed},
        {"world replay identical", test_world_replay_identical},
        {"world null safety", test_world_null},
        {"entity lifecycle", test_entity_lifecycle},
        {"entity capacity", test_entity_capacity},
        {"event stream order", test_event_stream_order},
        {"event stream wraparound", test_event_stream_wraparound},
        {"snapshot read-only and incremental", test_snapshot_readonly_and_incremental},
        {"framebuffer basics", test_framebuffer},
        {"framebuffer bad args", test_framebuffer_bad_args},
        {"raster draws pixels", test_raster_draws_expected_pixels},
        {"raster depth test", test_raster_depth_rejects_farther},
        {"raster tile-size determinism", test_raster_determinism},
    };
    return ws_test_run("world_render", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
