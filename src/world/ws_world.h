/* Worldseed Engine — world context (minimal P1 foundation).
 *
 * The world context owns world identity, seed and simulated time. It is the
 * single authority object; observation code receives read-only snapshots and
 * never writes here. This file grows only as later milestones require.
 */
#ifndef WS_WORLD_WORLD_H
#define WS_WORLD_WORLD_H

#include <stdint.h>

#include "../core/ws_rng.h"
#include "../core/ws_result.h"

#define WS_WORLD_SCHEMA_VERSION 1u

/* Fixed simulation time step in seconds (physics/authority tick). */
#define WS_WORLD_FIXED_DT 0.02

typedef struct ws_world {
    uint64_t world_uid;
    uint64_t seed;
    uint32_t schema_version;
    uint64_t tick;          /* number of fixed steps advanced */
    double time_seconds;    /* tick * WS_WORLD_FIXED_DT */
    ws_rng rng_world;       /* authoritative world stream */
} ws_world;

/* Create a world with the given UID and seed. */
ws_result ws_world_init(ws_world *world, uint64_t world_uid, uint64_t seed);

/* Advance the world by exactly one fixed step. */
ws_result ws_world_step(ws_world *world);

/* Advance by `steps` fixed steps. steps == 0 is a no-op. */
ws_result ws_world_advance(ws_world *world, uint64_t steps);

#endif /* WS_WORLD_WORLD_H */
