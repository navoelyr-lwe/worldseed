/* Worldseed Engine — versioned deterministic RNG stream.
 *
 * Algorithm: SplitMix64 output function applied to a 64-bit counter-based
 * state. The algorithm identity is fixed by WS_RNG_VERSION so that saved
 * worlds and replays remain reproducible across builds. Changing the
 * algorithm requires a new WS_RNG_VERSION and a migration note.
 *
 * This generator is for simulation only; it is not a cryptographic RNG.
 */
#ifndef WS_CORE_RNG_H
#define WS_CORE_RNG_H

#include <stdint.h>

#include "ws_result.h"

#define WS_RNG_VERSION 1u

typedef struct ws_rng {
    uint64_t state;
    uint64_t seed;
    uint64_t stream_id;
    uint64_t draws;
} ws_rng;

/* Initialise a stream from a world seed and a stream identifier.
 * Distinct stream identifiers yield independent, non-overlapping sequences. */
void ws_rng_init(ws_rng *rng, uint64_t world_seed, uint64_t stream_id);

/* Reset to the stream's origin, discarding draw history. */
void ws_rng_reset(ws_rng *rng);

/* Next raw 64-bit value. */
uint64_t ws_rng_next_u64(ws_rng *rng);

/* Uniform double in [0, 1). */
double ws_rng_next_double(ws_rng *rng);

/* Uniform integer in [0, bound). Returns WS_ERR_INVALID_ARGUMENT when bound == 0. */
ws_result ws_rng_next_bounded_u64(ws_rng *rng, uint64_t bound, uint64_t *out);

/* Save/restore the full stream state for checkpointing. */
ws_result ws_rng_save(const ws_rng *rng, uint64_t *out_state, uint64_t *out_draws);
ws_result ws_rng_load(ws_rng *rng, uint64_t world_seed, uint64_t stream_id,
                      uint64_t state, uint64_t draws);

#endif /* WS_CORE_RNG_H */
