/* Worldseed Engine — entity registry. */
#include "ws_entity.h"

#include <stddef.h>

ws_result ws_entity_registry_init(ws_entity_registry *registry,
                                  void *arena_block, size_t arena_size,
                                  uint32_t capacity)
{
    if (registry == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    registry->capacity = capacity;
    registry->next_uid = 1u;

    ws_result r = ws_arena_init(&registry->arena, arena_block, arena_size);
    if (r != WS_OK) {
        return r;
    }

    void *slots = ws_arena_alloc(&registry->arena, (size_t)capacity * sizeof(ws_entity),
                                 _Alignof(ws_entity));
    void *occupied = ws_arena_alloc(&registry->arena, capacity, 1u);
    void *generations = ws_arena_alloc(&registry->arena,
                                       (size_t)capacity * sizeof(uint32_t),
                                       _Alignof(uint32_t));
    if (slots == NULL || occupied == NULL || generations == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    return ws_slotmap_init(&registry->map, slots, occupied, generations, capacity,
                           sizeof(ws_entity));
}

ws_handle ws_entity_create(ws_entity_registry *registry, uint32_t kind, ws_vec3 position)
{
    if (registry == NULL) {
        return ws_handle_invalid();
    }
    ws_handle handle = ws_slotmap_insert(&registry->map);
    if (!ws_handle_is_valid(handle)) {
        return handle;
    }
    ws_entity *entity = (ws_entity *)ws_slotmap_get(&registry->map, handle);
    if (entity == NULL) {
        return ws_handle_invalid();
    }
    entity->uid = registry->next_uid++;
    entity->kind = kind;
    entity->alive = 1;
    entity->position = position;
    entity->rotation_y = 0.0;
    entity->scale = 1.0;
    return handle;
}

ws_entity *ws_entity_get(ws_entity_registry *registry, ws_handle handle)
{
    if (registry == NULL) {
        return NULL;
    }
    return (ws_entity *)ws_slotmap_get(&registry->map, handle);
}

ws_result ws_entity_destroy(ws_entity_registry *registry, ws_handle handle)
{
    if (registry == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_entity *entity = ws_entity_get(registry, handle);
    if (entity == NULL) {
        return WS_ERR_NOT_FOUND;
    }
    entity->alive = 0;
    return ws_slotmap_remove(&registry->map, handle);
}

uint32_t ws_entity_count(const ws_entity_registry *registry)
{
    return registry != NULL ? ws_slotmap_count(&registry->map) : 0u;
}
