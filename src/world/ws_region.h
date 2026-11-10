/* Worldseed Engine — region grid geometry (P8).
 *
 * The persistent world is a grid of fixed-size regions addressed by signed
 * integer coordinates, so negative coordinates and world edges need no special
 * cases (docs/04 "Hiyerarşik bölge kimliği ve yerel koordinatlar"). Each region
 * is `cells * cell_size` metres on a side; a world position maps to one region
 * plus a region-local offset, which is the floating-origin representation the
 * renderer and the terrain generator already use.
 *
 * Region identity is a pure function of the seed and the coordinates, so the
 * same region always regenerates identically and two neighbours agree along
 * their shared edge. Adjacency and distance are defined here so streaming and
 * the near/mid/far level assignment share one definition instead of each
 * inventing its own.
 */
#ifndef WS_WORLD_REGION_H
#define WS_WORLD_REGION_H

#include <stdint.h>

#include "../core/ws_result.h"

#define WS_REGION_VERSION 1u

typedef struct ws_region_config {
    uint64_t seed;
    uint32_t cells;     /* cells per region edge */
    double cell_size;   /* metres between samples */
} ws_region_config;

typedef struct ws_region_coord {
    int64_t x;
    int64_t y;
} ws_region_coord;

typedef struct ws_region_pos {
    ws_region_coord region;
    double local_x; /* metres from the region origin, 0 .. extent */
    double local_y;
} ws_region_pos;

ws_region_config ws_region_config_default(uint64_t seed);

/* Region side length in metres. */
double ws_region_extent(const ws_region_config *config);

ws_region_coord ws_region_coord_make(int64_t x, int64_t y);

/* Compare by coordinates (0 when equal). */
int ws_region_coord_equal(ws_region_coord a, ws_region_coord b);

/* Map a world position (metres, may be negative) to its region and region-local
 * offset. Local offsets are always in [0, extent). */
ws_region_pos ws_region_from_world(const ws_region_config *config, double wx, double wy);

/* World metres of a region-local position. */
void ws_region_to_world(const ws_region_config *config, ws_region_coord region,
                        double local_x, double local_y, double *out_wx, double *out_wy);

/* World metres of a region centre. */
void ws_region_center_world(const ws_region_config *config, ws_region_coord region,
                            double *out_wx, double *out_wy);

/* True when the two regions share an edge (4-neighbourhood, not diagonal). */
int ws_region_adjacent(ws_region_coord a, ws_region_coord b);

/* Chebyshev (king-move) distance in region units. */
int64_t ws_region_distance(ws_region_coord a, ws_region_coord b);

/* Deterministic 64-bit hash of (seed, coordinates) for stable per-region
 * features (population seeding, background variation). */
uint64_t ws_region_hash(const ws_region_config *config, ws_region_coord region);

#endif /* WS_WORLD_REGION_H */
