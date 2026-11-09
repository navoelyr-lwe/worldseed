/* Worldseed Engine — bump/arena allocator.
 *
 * A monotonic allocator over one caller-owned block. Allocation never frees
 * individual objects; the whole arena resets or is destroyed at once. All
 * allocations are aligned and size-checked. This is the default allocation
 * strategy for per-tick scratch and subsystem-lifetime data, which keeps
 * ownership explicit and fragmentation out of the hot path.
 */
#ifndef WS_CORE_ARENA_H
#define WS_CORE_ARENA_H

#include <stddef.h>
#include <stdint.h>

#include "ws_result.h"

/* Alignment attribute for an arena backing block. MSVC needs __declspec; the
 * other toolchains use C11 _Alignas. 16 covers the natural alignment of every
 * type the engine stores in an arena. */
#if defined(_MSC_VER)
#define WS_ARENA_BLOCK_ALIGN __declspec(align(16))
#else
#define WS_ARENA_BLOCK_ALIGN _Alignas(16)
#endif

typedef struct ws_arena {
    uint8_t *base;
    size_t capacity;
    size_t offset;
    size_t peak;
} ws_arena;

/* Initialise an arena over a caller-owned block. Does not take ownership.
 * `block` must be aligned to at least the largest alignment that will be
 * requested; WS_ARENA_BLOCK_ALIGN provides a portable attribute. Allocations
 * are aligned relative to the block base. */
ws_result ws_arena_init(ws_arena *arena, void *block, size_t capacity);

/* Allocate `size` bytes aligned to `alignment` (a power of two, or 0 for the
 * default). Returns NULL on failure or when size is 0. */
void *ws_arena_alloc(ws_arena *arena, size_t size, size_t alignment);

/* Zero-initialised allocation. */
void *ws_arena_alloc_zero(ws_arena *arena, size_t size, size_t alignment);

/* Reset to empty, reusing the block. */
void ws_arena_reset(ws_arena *arena);

size_t ws_arena_used(const ws_arena *arena);
size_t ws_arena_peak(const ws_arena *arena);
size_t ws_arena_remaining(const ws_arena *arena);

#endif /* WS_CORE_ARENA_H */
