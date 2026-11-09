/* Worldseed Engine — world context (minimal P1 foundation). */
#include "ws_world.h"

#include <stddef.h>

#define WS_RNG_STREAM_WORLD 1u

ws_result ws_world_init(ws_world *world, uint64_t world_uid, uint64_t seed)
{
    if (world == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    world->world_uid = world_uid;
    world->seed = seed;
    world->schema_version = WS_WORLD_SCHEMA_VERSION;
    world->tick = 0U;
    world->time_seconds = 0.0;
    ws_rng_init(&world->rng_world, seed, WS_RNG_STREAM_WORLD);
    return WS_OK;
}

ws_result ws_world_step(ws_world *world)
{
    if (world == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    /* A fixed step keeps the authority timeline deterministic and independent
     * of render frame rate. Subsystems are added in later milestones. */
    world->tick += 1U;
    world->time_seconds = (double)world->tick * WS_WORLD_FIXED_DT;
    return WS_OK;
}

ws_result ws_world_advance(ws_world *world, uint64_t steps)
{
    if (world == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    for (uint64_t i = 0; i < steps; ++i) {
        ws_result r = ws_world_step(world);
        if (r != WS_OK) {
            return r;
        }
    }
    return WS_OK;
}
