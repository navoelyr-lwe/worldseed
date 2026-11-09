/* Worldseed Engine — habitat classification.
 *
 * Habitats are derived fields, not hand-placed: each terrain cell is assigned a
 * habitat from height, slope, surface class and water, following docs/04. The
 * first release has one biome (temperate mixed forest) with four linked
 * habitats: forest, meadow/opening, freshwater edge and rocky slope. This keeps
 * the terrain, water, vegetation and animal layers referring to one shared
 * classification instead of each inventing its own.
 */
#ifndef WS_WORLD_HABITAT_H
#define WS_WORLD_HABITAT_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_hydro.h"
#include "ws_terrain.h"

#define WS_HABITAT_VERSION 1u

typedef enum ws_habitat {
    WS_HABITAT_ROCKY = 0,
    WS_HABITAT_FRESHWATER = 1,
    WS_HABITAT_FOREST = 2,
    WS_HABITAT_MEADOW = 3,
    WS_HABITAT_COUNT = 4
} ws_habitat;

typedef struct ws_habitat_map {
    uint32_t cells;
    uint8_t *kind;        /* cells^2 ws_habitat */
    uint32_t counts[WS_HABITAT_COUNT];
} ws_habitat_map;

/* Classify every cell. Allocates `kind`; the caller must call
 * ws_habitat_free. */
ws_result ws_habitat_create(ws_habitat_map *map, const ws_terrain *terrain,
                            const ws_hydro *hydro);

void ws_habitat_free(ws_habitat_map *map);

uint8_t ws_habitat_at(const ws_habitat_map *map, uint32_t cx, uint32_t cy);

/* Fraction of cells in the given habitat (0 when empty). */
double ws_habitat_fraction(const ws_habitat_map *map, uint8_t habitat);

const char *ws_habitat_name(uint8_t habitat);

#endif /* WS_WORLD_HABITAT_H */
