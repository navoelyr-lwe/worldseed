/* Worldseed Engine — bump/arena allocator. */
#include "ws_arena.h"

#include <string.h>

#define WS_ARENA_DEFAULT_ALIGNMENT 16u

static size_t ws_align_up(size_t value, size_t alignment)
{
    /* alignment is a power of two. Compute padding without adding alignment-1
     * to value, which could overflow. `& mask` makes an already-aligned value
     * produce zero padding. */
    size_t mask = alignment - 1u;
    size_t pad = (alignment - (value & mask)) & mask;
    return value + pad;
}

static int ws_is_power_of_two(size_t v)
{
    return v != 0u && (v & (v - 1u)) == 0u;
}

ws_result ws_arena_init(ws_arena *arena, void *block, size_t capacity)
{
    if (arena == NULL || block == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    arena->base = (uint8_t *)block;
    arena->capacity = capacity;
    arena->offset = 0u;
    arena->peak = 0u;
    return WS_OK;
}

void *ws_arena_alloc(ws_arena *arena, size_t size, size_t alignment)
{
    if (arena == NULL || arena->base == NULL || size == 0u) {
        return NULL;
    }
    if (alignment == 0u) {
        alignment = WS_ARENA_DEFAULT_ALIGNMENT;
    }
    if (!ws_is_power_of_two(alignment)) {
        return NULL;
    }
    if (arena->offset > arena->capacity) {
        return NULL;
    }
    size_t aligned = ws_align_up(arena->offset, alignment);
    if (aligned > arena->capacity) {
        return NULL;
    }
    size_t remaining = arena->capacity - aligned;
    if (size > remaining) {
        return NULL;
    }
    void *result = arena->base + aligned;
    arena->offset = aligned + size;
    if (arena->offset > arena->peak) {
        arena->peak = arena->offset;
    }
    return result;
}

void *ws_arena_alloc_zero(ws_arena *arena, size_t size, size_t alignment)
{
    void *p = ws_arena_alloc(arena, size, alignment);
    if (p != NULL) {
        memset(p, 0, size);
    }
    return p;
}

void ws_arena_reset(ws_arena *arena)
{
    if (arena != NULL) {
        arena->offset = 0u;
    }
}

size_t ws_arena_used(const ws_arena *arena)
{
    return arena != NULL ? arena->offset : 0u;
}

size_t ws_arena_peak(const ws_arena *arena)
{
    return arena != NULL ? arena->peak : 0u;
}

size_t ws_arena_remaining(const ws_arena *arena)
{
    if (arena == NULL || arena->offset > arena->capacity) {
        return 0u;
    }
    return arena->capacity - arena->offset;
}
