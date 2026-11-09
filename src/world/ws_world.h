/* Worldseed Engine — world context (P1 engine core).
 *
 * The world context is the single authority object. It owns the deterministic
 * timeline, the world RNG stream, the entity registry and the event stream.
 * Observation code receives read-only snapshots and never writes here.
 *
 * All storage is caller-owned: the world never allocates. This keeps memory
 * policy explicit and makes deterministic replay reproducible.
 */
#ifndef WS_WORLD_WORLD_H
#define WS_WORLD_WORLD_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_rng.h"
#include "../core/ws_result.h"
#include "ws_entity.h"
#include "ws_event.h"

#define WS_WORLD_SCHEMA_VERSION 1u

/* Fixed simulation time step in seconds (authority tick). */
#define WS_WORLD_FIXED_DT 0.02

typedef struct ws_world_config {
    void *arena_block;         /* entity storage; owned by the caller */
    size_t arena_size;
    uint32_t entity_capacity;
    ws_event *event_storage;   /* event ring storage; owned by the caller */
    uint32_t event_capacity;
} ws_world_config;

typedef struct ws_world {
    uint64_t world_uid;
    uint64_t seed;
    uint32_t schema_version;
    uint64_t tick;
    double time_seconds;
    ws_rng rng_world;          /* authoritative world stream */
    ws_entity_registry entities;
    ws_event_log events;
} ws_world;

/* Create a world. config and all its storage must outlive the world. */
ws_result ws_world_init(ws_world *world, const ws_world_config *config,
                        uint64_t world_uid, uint64_t seed);

/* Advance the world by exactly one fixed step. */
ws_result ws_world_step(ws_world *world);

/* Advance by `steps` fixed steps. steps == 0 is a no-op. */
ws_result ws_world_advance(ws_world *world, uint64_t steps);

#endif /* WS_WORLD_WORLD_H */
