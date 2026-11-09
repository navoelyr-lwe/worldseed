/* Worldseed Engine — generation-checked slot map. */
#include "ws_slotmap.h"

#include <string.h>

#define WS_SLOTMAP_INITIAL_GENERATION 1u

ws_result ws_slotmap_init(ws_slotmap *map,
                          void *slot_bytes, void *occupied_bytes, void *generation_bytes,
                          uint32_t capacity, size_t stride)
{
    if (map == NULL || slot_bytes == NULL || occupied_bytes == NULL ||
        generation_bytes == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (capacity == 0u || stride == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    map->slots = (uint8_t *)slot_bytes;
    map->occupied = (uint8_t *)occupied_bytes;
    map->generation = (uint32_t *)generation_bytes;
    map->capacity = capacity;
    map->count = 0u;
    map->stride = stride;
    map->free_hint = 0u;

    memset(map->slots, 0, (size_t)capacity * stride);
    memset(map->occupied, 0, capacity);
    for (uint32_t i = 0; i < capacity; ++i) {
        map->generation[i] = WS_SLOTMAP_INITIAL_GENERATION;
    }
    return WS_OK;
}

static uint8_t *ws_slotmap_slot_at(const ws_slotmap *map, uint32_t index)
{
    return map->slots + (size_t)index * map->stride;
}

ws_handle ws_slotmap_insert(ws_slotmap *map)
{
    if (map == NULL) {
        return ws_handle_invalid();
    }
    for (uint32_t scanned = 0; scanned < map->capacity; ++scanned) {
        uint32_t index = map->free_hint;
        map->free_hint = (map->free_hint + 1u) % map->capacity;
        if (map->occupied[index] == 0u) {
            map->occupied[index] = 1u;
            map->count += 1u;
            memset(ws_slotmap_slot_at(map, index), 0, map->stride);
            ws_handle handle = {index, map->generation[index]};
            return handle;
        }
    }
    return ws_handle_invalid();
}

void *ws_slotmap_get(const ws_slotmap *map, ws_handle handle)
{
    if (map == NULL) {
        return NULL;
    }
    if (handle.index >= map->capacity) {
        return NULL;
    }
    if (map->occupied[handle.index] == 0u) {
        return NULL;
    }
    if (map->generation[handle.index] != handle.generation) {
        return NULL;
    }
    return ws_slotmap_slot_at(map, handle.index);
}

ws_result ws_slotmap_remove(ws_slotmap *map, ws_handle handle)
{
    if (map == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (handle.index >= map->capacity) {
        return WS_ERR_NOT_FOUND;
    }
    if (map->occupied[handle.index] == 0u ||
        map->generation[handle.index] != handle.generation) {
        return WS_ERR_NOT_FOUND;
    }
    map->occupied[handle.index] = 0u;
    map->count -= 1u;
    /* Bump the generation so the released handle can never resolve again. */
    map->generation[handle.index] += 1u;
    if (map->generation[handle.index] == WS_HANDLE_INVALID_GENERATION) {
        map->generation[handle.index] += 1u;
    }
    if (handle.index < map->free_hint) {
        map->free_hint = handle.index;
    }
    return WS_OK;
}

uint32_t ws_slotmap_count(const ws_slotmap *map)
{
    return map != NULL ? map->count : 0u;
}

uint32_t ws_slotmap_capacity(const ws_slotmap *map)
{
    return map != NULL ? map->capacity : 0u;
}
