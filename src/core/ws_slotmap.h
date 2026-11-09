/* Worldseed Engine — generation-checked slot map.
 *
 * Maps stable 32-bit ids to caller-owned slots of fixed stride. Each id packs
 * a slot index and a generation; freeing a slot bumps its generation so stale
 * ids can never resolve to a reused slot. This is the storage behind world
 * object handles.
 *
 * The map does not own slot memory: the caller supplies a block of
 * capacity * stride bytes. This keeps allocation policy with the world.
 */
#ifndef WS_CORE_SLOTMAP_H
#define WS_CORE_SLOTMAP_H

#include <stddef.h>
#include <stdint.h>

#include "ws_handle.h"
#include "ws_result.h"

typedef struct ws_slotmap {
    uint8_t *slots;       /* capacity * stride bytes */
    uint8_t *occupied;    /* capacity bytes; non-zero when live */
    uint32_t *generation; /* capacity entries; starts at 1 */
    uint32_t capacity;
    uint32_t count;
    size_t stride;
    uint32_t free_hint;   /* first index that may be free; a scan hint only */
} ws_slotmap;

/* Initialise over caller-owned memory. The three blocks must each be large
 * enough for `capacity` entries: slot_bytes is capacity*stride bytes,
 * occupied_bytes is capacity bytes and generation_bytes is
 * capacity*sizeof(uint32_t) bytes. */
ws_result ws_slotmap_init(ws_slotmap *map,
                          void *slot_bytes, void *occupied_bytes, void *generation_bytes,
                          uint32_t capacity, size_t stride);

/* Reserve a slot and return a fresh handle. Returns the invalid handle when
 * full. */
ws_handle ws_slotmap_insert(ws_slotmap *map);

/* Resolve a handle to its slot. Returns NULL when stale or out of range. */
void *ws_slotmap_get(const ws_slotmap *map, ws_handle handle);

/* Release a slot, bumping its generation. Returns WS_ERR_NOT_FOUND when stale. */
ws_result ws_slotmap_remove(ws_slotmap *map, ws_handle handle);

uint32_t ws_slotmap_count(const ws_slotmap *map);
uint32_t ws_slotmap_capacity(const ws_slotmap *map);

#endif /* WS_CORE_SLOTMAP_H */
