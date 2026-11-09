/* Worldseed Engine — robustness/fuzz tests.
 *
 * Feed random and truncated byte streams to the reader and event log and
 * assert that every call returns a defined result and never crashes or reads
 * out of bounds. This runs under ASan/UBSan in CI.
 */
#include "ws_test.h"

#include "core/ws_rng.h"
#include "core/ws_serialize.h"
#include "world/ws_event.h"

#include <stdint.h>
#include <string.h>

static int test_reader_random_bytes(void)
{
    ws_rng rng;
    ws_rng_init(&rng, 0xF0F0F0F0ULL, 1ULL);

    unsigned char buffer[64];
    for (int iter = 0; iter < 5000; ++iter) {
        uint32_t len = (uint32_t)ws_rng_next_u64(&rng) % (sizeof(buffer) + 1u);
        for (uint32_t i = 0; i < len; ++i) {
            buffer[i] = (unsigned char)(ws_rng_next_u64(&rng) & 0xFFu);
        }
        ws_reader r;
        ws_reader_init(&r, buffer, len);

        /* Walk the stream reading typed fields until exhausted. Each call must
         * return either WS_OK or WS_ERR_PARSE, never anything else, and the
         * offset must never exceed the length. */
        for (int guard = 0; guard < 64; ++guard) {
            uint64_t v = 0;
            ws_result res = ws_reader_u64(&r, &v);
            WS_CHECK(res == WS_OK || res == WS_ERR_PARSE);
            WS_CHECK(r.offset <= r.length);
            if (res != WS_OK) {
                break;
            }
        }
    }
    return 1;
}

static int test_reader_truncated_each_width(void)
{
    unsigned char buffer[16];
    for (size_t i = 0; i < sizeof(buffer); ++i) {
        buffer[i] = (unsigned char)i;
    }
    /* A reader over each truncated length must reject the wide read. */
    for (size_t len = 0; len < 8u; ++len) {
        ws_reader r;
        ws_reader_init(&r, buffer, len);
        uint64_t v = 0;
        WS_CHECK(ws_reader_u64(&r, &v) == WS_ERR_PARSE);
    }
    ws_reader ok;
    ws_reader_init(&ok, buffer, sizeof(buffer));
    uint64_t v = 0;
    WS_CHECK(ws_reader_u64(&ok, &v) == WS_OK);
    return 1;
}

static int test_reader_null_guards(void)
{
    uint32_t v = 0;
    WS_CHECK(ws_reader_u32(NULL, &v) == WS_ERR_NULL_POINTER);
    ws_reader r;
    ws_reader_init(&r, NULL, 0u);
    WS_CHECK(ws_reader_u32(&r, &v) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_reader_u32(&r, NULL) == WS_ERR_NULL_POINTER);
    return 1;
}

static int test_writer_reports_capacity(void)
{
    unsigned char buffer[3];
    ws_writer w;
    ws_writer_init(&w, buffer, sizeof(buffer));
    WS_CHECK(ws_writer_bytes(&w, "ab", 2u) == WS_OK);
    WS_CHECK(ws_writer_bytes(&w, "cd", 2u) == WS_ERR_CAPACITY);
    WS_CHECK(ws_writer_size(&w) == 2u);
    WS_CHECK(ws_writer_bytes(&w, NULL, 0u) == WS_OK);
    WS_CHECK(ws_writer_bytes(&w, NULL, 1u) == WS_ERR_NULL_POINTER);
    return 1;
}

static int test_event_push_guards(void)
{
    ws_event storage[4];
    ws_event_log log;
    WS_CHECK(ws_event_log_init(&log, storage, 4u) == WS_OK);

    uint8_t too_big[WS_EVENT_PAYLOAD_MAX + 1u];
    memset(too_big, 0, sizeof(too_big));
    WS_CHECK(ws_event_log_push(&log, WS_EVENT_CUSTOM, 0u, too_big, sizeof(too_big)) ==
             WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_event_log_push(&log, WS_EVENT_CUSTOM, 0u, NULL, 4u) == WS_ERR_NULL_POINTER);

    /* A stream of pushes must never corrupt the count beyond capacity. */
    ws_rng rng;
    ws_rng_init(&rng, 5ULL, 2ULL);
    for (int i = 0; i < 1000; ++i) {
        uint16_t size = (uint16_t)(ws_rng_next_u64(&rng) % (WS_EVENT_PAYLOAD_MAX + 1u));
        uint8_t payload[WS_EVENT_PAYLOAD_MAX];
        memset(payload, 0, sizeof(payload));
        WS_CHECK(ws_event_log_push(&log, WS_EVENT_CUSTOM, (uint64_t)i, payload, size) == WS_OK);
        WS_CHECK(ws_event_log_count(&log) <= 4u);
    }
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"reader random bytes", test_reader_random_bytes},
        {"reader truncated widths", test_reader_truncated_each_width},
        {"reader null guards", test_reader_null_guards},
        {"writer capacity reporting", test_writer_reports_capacity},
        {"event push guards", test_event_push_guards},
    };
    return ws_test_run("fuzz", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
