/* Worldseed Engine — procedural terrain regions.
 *
 * Terrain is generated deterministically from the world seed and the region
 * coordinate. The height field is a pure function of world position in metres,
 * so two adjacent regions sample the exact same height along their shared edge
 * and stitch without a seam. Regions are addressed by signed integer
 * coordinates, so negative coordinates and world edges work without special
 * cases, and the renderer/camera can keep region-local (floating-origin)
 * positions without losing global precision.
 *
 * Surface classes describe the ground material; water is layered on top by the
 * hydro module (ws_hydro). Erosion and full geology are out of scope here; the
 * first release represents relief with fractal noise only, and that limitation
 * is recorded in the model card.
 */
#ifndef WS_WORLD_TERRAIN_H
#define WS_WORLD_TERRAIN_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"

/* Bump when the generator changes so saved worlds can be detected as stale. */
#define WS_TERRAIN_VERSION 1u

typedef enum ws_surface_class {
    WS_SURFACE_GRASS = 0,
    WS_SURFACE_SOIL = 1,
    WS_SURFACE_ROCK = 2,
    WS_SURFACE_SAND = 3,
    WS_SURFACE_CLASS_COUNT = 4
} ws_surface_class;

typedef struct ws_terrain_config {
    uint64_t seed;
    uint32_t version;
    double cell_size;      /* metres between height samples */
    uint32_t region_cells; /* cells per region edge */
    double base_height;
    double hill_amplitude;
    double mountain_amplitude;
    double water_level;    /* absolute metres; used for shoreline and hydro */
} ws_terrain_config;

typedef struct ws_terrain {
    ws_terrain_config config;
    int64_t region_x;
    int64_t region_y;
    uint32_t cells;
    double origin_x;       /* world metres of sample (0,0) */
    double origin_y;
    double *heights;       /* (cells + 1)^2, row-major, metres */
    double *slope;         /* cells^2, rise/run at the cell centre */
    uint8_t *surface;      /* cells^2, ws_surface_class */
} ws_terrain;

/* A sensible default configuration for the first temperate ecosystem. */
ws_terrain_config ws_terrain_config_default(uint64_t seed);

/* Height in metres at a world position. Pure and deterministic; independent of
 * any generated region. */
double ws_terrain_height_at(const ws_terrain_config *config, double wx, double wy);

/* Generate the region at signed coordinates (rx, ry). Allocates the sample
 * arrays; the caller must call ws_terrain_free. */
ws_result ws_terrain_create(ws_terrain *terrain, const ws_terrain_config *config,
                            int64_t rx, int64_t ry);

void ws_terrain_free(ws_terrain *terrain);

/* Height sample at grid node (ix, iy) in [0, cells]. */
double ws_terrain_node(const ws_terrain *terrain, uint32_t ix, uint32_t iy);

/* Bilinear height at world metres (wx, wy). */
double ws_terrain_height_bilinear(const ws_terrain *terrain, double wx, double wy);

/* Cell-centre surface class and slope (cx, cy in [0, cells)). */
uint8_t ws_terrain_cell_surface(const ws_terrain *terrain, uint32_t cx, uint32_t cy);
double ws_terrain_cell_slope(const ws_terrain *terrain, uint32_t cx, uint32_t cy);

/* Surface class name, for logs and manifests. */
const char *ws_surface_class_name(uint8_t surface);

#endif /* WS_WORLD_TERRAIN_H */
