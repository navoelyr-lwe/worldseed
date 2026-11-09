/* Worldseed Engine — core unit tests: result, math, handle, rng, hash,
 * arena, slot map and serialization. */
#include "ws_test.h"

#include "core/ws_arena.h"
#include "core/ws_handle.h"
#include "core/ws_hash.h"
#include "core/ws_math.h"
#include "core/ws_result.h"
#include "core/ws_rng.h"
#include "core/ws_serialize.h"
#include "core/ws_slotmap.h"

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int test_result_names(void)
{
    WS_CHECK(strcmp(ws_result_name(WS_OK), "WS_OK") == 0);
    WS_CHECK(strcmp(ws_result_name(WS_ERR_OUT_OF_MEMORY), "WS_ERR_OUT_OF_MEMORY") == 0);
    WS_CHECK(ws_result_is_ok(WS_OK) == 1);
    WS_CHECK(ws_result_is_ok(WS_ERR_IO) == 0);
    return 1;
}

static int test_math_primitives(void)
{
    WS_CHECK(ws_clamp_double(5.0, 0.0, 1.0) == 1.0);
    WS_CHECK(ws_clamp_double(-5.0, 0.0, 1.0) == 0.0);
    WS_CHECK(fabs(ws_lerp(0.0, 10.0, 0.25) - 2.5) < 1e-12);

    ws_vec2 a = ws_vec2_make(3.0, 4.0);
    WS_CHECK(fabs(ws_vec2_length(a) - 5.0) < 1e-12);
    ws_vec2 b = ws_vec2_make(1.0, 0.0);
    WS_CHECK(fabs(ws_vec2_dot(a, b) - 3.0) < 1e-12);

    ws_vec3 v = ws_vec3_make(1.0, 2.0, 2.0);
    WS_CHECK(fabs(ws_vec3_length(v) - 3.0) < 1e-12);
    return 1;
}

static int test_mul_overflow(void)
{
    size_t out = 0;
    WS_CHECK(ws_mul_size_checked(4u, 8u, &out) == 1);
    WS_CHECK(out == 32u);
    WS_CHECK(ws_mul_size_checked((size_t)-1, 2u, &out) == 0);
    WS_CHECK(ws_mul_size_checked(1u, 0u, &out) == 1 && out == 0u);
    WS_CHECK(ws_mul_size_checked(4u, 8u, NULL) == 0);
    return 1;
}

static int test_handles(void)
{
    ws_handle invalid = ws_handle_invalid();
    WS_CHECK(!ws_handle_is_valid(invalid));
    ws_handle h = {5u, 1u};
    WS_CHECK(ws_handle_is_valid(h));
    WS_CHECK(ws_handle_equal(h, h));
    ws_handle other = {5u, 2u};
    WS_CHECK(!ws_handle_equal(h, other));
    return 1;
}

static int test_rng_determinism(void)
{
    ws_rng a;
    ws_rng b;
    ws_rng_init(&a, 12345ULL, 1ULL);
    ws_rng_init(&b, 12345ULL, 1ULL);
    for (int i = 0; i < 1000; ++i) {
        WS_CHECK(ws_rng_next_u64(&a) == ws_rng_next_u64(&b));
    }

    ws_rng c;
    ws_rng_init(&c, 12345ULL, 2ULL);
    WS_CHECK(ws_rng_next_u64(&b) != ws_rng_next_u64(&c));
    return 1;
}

static int test_rng_range_and_bounds(void)
{
    ws_rng rng;
    ws_rng_init(&rng, 777ULL, 3ULL);
    for (int i = 0; i < 10000; ++i) {
        double d = ws_rng_next_double(&rng);
        WS_CHECK(d >= 0.0 && d < 1.0);
    }

    uint64_t v = 99u;
    WS_CHECK(ws_rng_next_bounded_u64(&rng, 0u, &v) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_rng_next_bounded_u64(&rng, 10u, &v) == WS_OK);
    WS_CHECK(v < 10u);
    return 1;
}

static int test_rng_save_load(void)
{
    ws_rng rng;
    ws_rng_init(&rng, 42ULL, 9ULL);
    (void)ws_rng_next_u64(&rng);
    (void)ws_rng_next_u64(&rng);

    uint64_t state = 0;
    uint64_t draws = 0;
    WS_CHECK(ws_rng_save(&rng, &state, &draws) == WS_OK);
    uint64_t expected = ws_rng_next_u64(&rng);

    ws_rng restored;
    WS_CHECK(ws_rng_load(&restored, 42ULL, 9ULL, state, draws) == WS_OK);
    WS_CHECK(ws_rng_next_u64(&restored) == expected);
    return 1;
}

static int test_sha256_vectors(void)
{
    char hex[WS_SHA256_DIGEST_SIZE * 2u + 1u];
    uint8_t digest[WS_SHA256_DIGEST_SIZE];

    ws_sha256_buffer("", 0u, digest);
    ws_sha256_hex(digest, hex);
    WS_CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855") == 0);

    ws_sha256_buffer("abc", 3u, digest);
    ws_sha256_hex(digest, hex);
    WS_CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0);

    const char *msg = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    ws_sha256_buffer(msg, strlen(msg), digest);
    ws_sha256_hex(digest, hex);
    WS_CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1") == 0);
    return 1;
}

static int test_sha256_streaming(void)
{
    const char *msg = "The quick brown fox jumps over the lazy dog";
    size_t len = strlen(msg);
    uint8_t one_shot[WS_SHA256_DIGEST_SIZE];
    ws_sha256_buffer(msg, len, one_shot);

    ws_sha256 ctx;
    ws_sha256_init(&ctx);
    for (size_t i = 0; i < len; i += 7u) {
        size_t chunk = (len - i) < 7u ? (len - i) : 7u;
        ws_sha256_update(&ctx, msg + i, chunk);
    }
    uint8_t streamed[WS_SHA256_DIGEST_SIZE];
    ws_sha256_final(&ctx, streamed);
    WS_CHECK(memcmp(one_shot, streamed, WS_SHA256_DIGEST_SIZE) == 0);
    return 1;
}

static int test_arena_alignment_and_bounds(void)
{
    static _Alignas(max_align_t) unsigned char block[256];
    ws_arena arena;
    WS_CHECK(ws_arena_init(&arena, block, sizeof(block)) == WS_OK);
    WS_CHECK(ws_arena_used(&arena) == 0u);

    void *a = ws_arena_alloc(&arena, 10u, 8u);
    WS_CHECK(a != NULL);
    WS_CHECK(((uintptr_t)a % 8u) == 0u);

    void *b = ws_arena_alloc(&arena, 3u, 1u);
    WS_CHECK(b != NULL);
    WS_CHECK(b > a);

    /* Request larger than the remaining capacity must fail, not wrap. */
    WS_CHECK(ws_arena_alloc(&arena, sizeof(block), 1u) == NULL);
    WS_CHECK(ws_arena_remaining(&arena) <= sizeof(block));

    size_t peak = ws_arena_peak(&arena);
    WS_CHECK(peak > 0u);
    ws_arena_reset(&arena);
    WS_CHECK(ws_arena_used(&arena) == 0u);
    WS_CHECK(ws_arena_peak(&arena) == peak);

    WS_CHECK(ws_arena_alloc(&arena, 4u, 3u) == NULL); /* non power-of-two */
    WS_CHECK(ws_arena_alloc(&arena, 0u, 1u) == NULL);
    return 1;
}

static int test_slotmap_lifecycle(void)
{
    enum { CAP = 8 };
    static unsigned char slots[CAP * 16u];
    static unsigned char occupied[CAP];
    static uint32_t generations[CAP];

    ws_slotmap map;
    WS_CHECK(ws_slotmap_init(&map, slots, occupied, generations, CAP, 16u) == WS_OK);
    WS_CHECK(ws_slotmap_capacity(&map) == CAP);
    WS_CHECK(ws_slotmap_count(&map) == 0u);

    ws_handle h = ws_slotmap_insert(&map);
    WS_CHECK(ws_handle_is_valid(h));
    WS_CHECK(ws_slotmap_count(&map) == 1u);
    void *p = ws_slotmap_get(&map, h);
    WS_CHECK(p != NULL);
    memset(p, 0xAB, 16u);
    WS_CHECK(ws_slotmap_get(&map, h) == p);

    WS_CHECK(ws_slotmap_remove(&map, h) == WS_OK);
    WS_CHECK(ws_slotmap_count(&map) == 0u);
    /* The stale handle must no longer resolve. */
    WS_CHECK(ws_slotmap_get(&map, h) == NULL);
    WS_CHECK(ws_slotmap_remove(&map, h) == WS_ERR_NOT_FOUND);

    ws_handle h2 = ws_slotmap_insert(&map);
    WS_CHECK(ws_handle_is_valid(h2));
    WS_CHECK(h2.index == h.index);
    WS_CHECK(h2.generation != h.generation);
    WS_CHECK(ws_slotmap_get(&map, h) == NULL); /* old handle still stale */

    ws_handle bogus = {999u, 1u};
    WS_CHECK(ws_slotmap_get(&map, bogus) == NULL);
    return 1;
}

static int test_slotmap_full(void)
{
    enum { CAP = 4 };
    static unsigned char slots[CAP * 8u];
    static unsigned char occupied[CAP];
    static uint32_t generations[CAP];
    ws_slotmap map;
    WS_CHECK(ws_slotmap_init(&map, slots, occupied, generations, CAP, 8u) == WS_OK);
    for (int i = 0; i < CAP; ++i) {
        WS_CHECK(ws_handle_is_valid(ws_slotmap_insert(&map)));
    }
    WS_CHECK(!ws_handle_is_valid(ws_slotmap_insert(&map)));
    WS_CHECK(ws_slotmap_count(&map) == CAP);
    return 1;
}

static int test_serialize_roundtrip(void)
{
    unsigned char buffer[64];
    ws_writer w;
    ws_writer_init(&w, buffer, sizeof(buffer));
    WS_CHECK(ws_writer_u8(&w, 0x12u) == WS_OK);
    WS_CHECK(ws_writer_u16(&w, 0x3456u) == WS_OK);
    WS_CHECK(ws_writer_u32(&w, 0x789ABCDEu) == WS_OK);
    WS_CHECK(ws_writer_u64(&w, 0x1122334455667788ULL) == WS_OK);
    WS_CHECK(ws_writer_f64(&w, 3.5) == WS_OK);
    size_t size = ws_writer_size(&w);

    ws_reader r;
    ws_reader_init(&r, buffer, size);
    uint8_t v8 = 0;
    uint16_t v16 = 0;
    uint32_t v32 = 0;
    uint64_t v64 = 0;
    double vf = 0.0;
    WS_CHECK(ws_reader_u8(&r, &v8) == WS_OK && v8 == 0x12u);
    WS_CHECK(ws_reader_u16(&r, &v16) == WS_OK && v16 == 0x3456u);
    WS_CHECK(ws_reader_u32(&r, &v32) == WS_OK && v32 == 0x789ABCDEu);
    WS_CHECK(ws_reader_u64(&r, &v64) == WS_OK && v64 == 0x1122334455667788ULL);
    WS_CHECK(ws_reader_f64(&r, &vf) == WS_OK && vf == 3.5);
    WS_CHECK(ws_reader_remaining(&r) == 0u);
    return 1;
}

static int test_serialize_little_endian(void)
{
    unsigned char buffer[8];
    ws_writer w;
    ws_writer_init(&w, buffer, sizeof(buffer));
    WS_CHECK(ws_writer_u32(&w, 0x01020304u) == WS_OK);
    WS_CHECK(buffer[0] == 0x04u && buffer[1] == 0x03u &&
             buffer[2] == 0x02u && buffer[3] == 0x01u);
    return 1;
}

static int test_serialize_bounds(void)
{
    unsigned char buffer[4];
    ws_writer w;
    ws_writer_init(&w, buffer, sizeof(buffer));
    WS_CHECK(ws_writer_u32(&w, 1u) == WS_OK);
    WS_CHECK(ws_writer_u8(&w, 1u) == WS_ERR_CAPACITY); /* full */

    ws_reader r;
    ws_reader_init(&r, buffer, 2u);
    uint32_t v = 0;
    WS_CHECK(ws_reader_u32(&r, &v) == WS_ERR_PARSE); /* truncated */
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"result names", test_result_names},
        {"math primitives", test_math_primitives},
        {"checked multiply", test_mul_overflow},
        {"handles", test_handles},
        {"rng determinism", test_rng_determinism},
        {"rng range and bounds", test_rng_range_and_bounds},
        {"rng save/load", test_rng_save_load},
        {"sha256 vectors", test_sha256_vectors},
        {"sha256 streaming", test_sha256_streaming},
        {"arena alignment and bounds", test_arena_alignment_and_bounds},
        {"slotmap lifecycle", test_slotmap_lifecycle},
        {"slotmap full", test_slotmap_full},
        {"serialize roundtrip", test_serialize_roundtrip},
        {"serialize little-endian", test_serialize_little_endian},
        {"serialize bounds", test_serialize_bounds},
    };
    return ws_test_run("core", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
