/* Worldseed Engine — deterministic plant placement.
 *
 * Plants are placed per cell from the habitat classification and a
 * deterministic hash of (seed, cell), so the same world seed always yields the
 * same layout and placement does not depend on iteration order. The first
 * release uses a few functional groups (grass tuft, shrub, tree) as a stand-in
 * for the plant species model described in docs/04; growth over time and
 * resource uptake are later work.
 */
#ifndef WS_WORLD_VEGETATION_H
#define WS_WORLD_VEGETATION_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "../core/ws_math.h"
#include "ws_habitat.h"
#include "ws_terrain.h"

#define WS_VEGETATION_VERSION 1u

typedef enum ws_plant_kind {
    WS_PLANT_GRASS = 0,
    WS_PLANT_SHRUB = 1,
    WS_PLANT_TREE = 2,
    WS_PLANT_KIND_COUNT = 3
} ws_plant_kind;

typedef struct ws_plant {
    ws_vec2 position; /* world metres (x, y) */
    float height;     /* metres */
    uint8_t kind;     /* ws_plant_kind */
} ws_plant;

typedef struct ws_vegetation {
    ws_plant *plants;
    uint32_t count;
    uint32_t capacity;
    uint32_t counts[WS_PLANT_KIND_COUNT];
} ws_vegetation;

/* Scatter plants over the region. Allocates the plant array; the caller must
 * call ws_vegetation_free. */
ws_result ws_vegetation_create(ws_vegetation *veg, const ws_terrain *terrain,
                               const ws_habitat_map *habitat);

void ws_vegetation_free(ws_vegetation *veg);

const char *ws_plant_kind_name(uint8_t kind);

#endif /* WS_WORLD_VEGETATION_H */
