/* Worldseed Engine — entity registry.
 *
 * The registry owns the storage for world entities and hands out stable,
 * generation-checked handles. It is the first concrete user of the arena and
 * slot map, and the place world objects are created and destroyed.
 */
#ifndef WS_WORLD_ENTITY_H
#define WS_WORLD_ENTITY_H

#include <stdint.h>

#include "../core/ws_arena.h"
#include "../core/ws_handle.h"
#include "../core/ws_math.h"
#include "../core/ws_slotmap.h"
#include "../core/ws_result.h"

typedef struct ws_entity {
    uint64_t uid;
    uint32_t kind;
    int alive;
    ws_vec3 position;
    double rotation_y;
    double scale;
} ws_entity;

typedef struct ws_entity_registry {
    ws_arena arena;
    ws_slotmap map;
    uint64_t next_uid;
    uint32_t capacity;
} ws_entity_registry;

/* Create a registry with room for `capacity` entities. All storage comes from
 * `arena_block` of `arena_size` bytes, which must be aligned as described for
 * ws_arena_init (WS_ARENA_BLOCK_ALIGN). */
ws_result ws_entity_registry_init(ws_entity_registry *registry,
                                  void *arena_block, size_t arena_size,
                                  uint32_t capacity);

/* Create an entity of `kind` at `position`; returns its handle. */
ws_handle ws_entity_create(ws_entity_registry *registry, uint32_t kind, ws_vec3 position);

/* Resolve a handle to an entity, or NULL when stale/unknown. */
ws_entity *ws_entity_get(ws_entity_registry *registry, ws_handle handle);

/* Destroy an entity. Returns WS_ERR_NOT_FOUND when the handle is stale. */
ws_result ws_entity_destroy(ws_entity_registry *registry, ws_handle handle);

uint32_t ws_entity_count(const ws_entity_registry *registry);

#endif /* WS_WORLD_ENTITY_H */
