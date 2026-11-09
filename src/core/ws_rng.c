/* Worldseed Engine — versioned deterministic RNG stream. */
#include "ws_rng.h"

#include <stddef.h>

/* SplitMix64 finalizer (Steele et al., 2014). */
static uint64_t ws_splitmix64(uint64_t *state)
{
    uint64_t z = (*state += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

/* Derive a per-stream origin so that streams from one seed do not overlap. */
static uint64_t ws_stream_origin(uint64_t world_seed, uint64_t stream_id)
{
    uint64_t mix = world_seed ^ (stream_id * 0xD1B54A32D192ED03ULL);
    return ws_splitmix64(&mix);
}

void ws_rng_init(ws_rng *rng, uint64_t world_seed, uint64_t stream_id)
{
    if (rng == NULL) {
        return;
    }
    rng->seed = world_seed;
    rng->stream_id = stream_id;
    rng->state = ws_stream_origin(world_seed, stream_id);
    rng->draws = 0U;
}

void ws_rng_reset(ws_rng *rng)
{
    if (rng == NULL) {
        return;
    }
    rng->state = ws_stream_origin(rng->seed, rng->stream_id);
    rng->draws = 0U;
}

uint64_t ws_rng_next_u64(ws_rng *rng)
{
    if (rng == NULL) {
        return 0U;
    }
    rng->draws += 1U;
    return ws_splitmix64(&rng->state);
}

double ws_rng_next_double(ws_rng *rng)
{
    /* Use the top 53 bits for a value in [0, 1). */
    uint64_t v = ws_rng_next_u64(rng);
    return (double)(v >> 11) * (1.0 / 9007199254740992.0);
}

ws_result ws_rng_next_bounded_u64(ws_rng *rng, uint64_t bound, uint64_t *out)
{
    if (rng == NULL || out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (bound == 0U) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    /* Rejection sampling avoids modulo bias. */
    uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
    uint64_t value;
    do {
        value = ws_rng_next_u64(rng);
    } while (value >= limit);
    *out = value % bound;
    return WS_OK;
}

ws_result ws_rng_save(const ws_rng *rng, uint64_t *out_state, uint64_t *out_draws)
{
    if (rng == NULL || out_state == NULL || out_draws == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    *out_state = rng->state;
    *out_draws = rng->draws;
    return WS_OK;
}

ws_result ws_rng_load(ws_rng *rng, uint64_t world_seed, uint64_t stream_id,
                      uint64_t state, uint64_t draws)
{
    if (rng == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    rng->seed = world_seed;
    rng->stream_id = stream_id;
    rng->state = state;
    rng->draws = draws;
    return WS_OK;
}
