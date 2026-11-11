/* Worldseed Engine — screen-space tile binning for the OpenCL raster path (P12).
 *
 * The reference OpenCL raster kernel (ws_raster.cl) has each pixel work-item
 * walk the entire triangle list, so cost grows as pixels x triangles and does
 * not scale to a full scene. This module builds the host-side acceleration
 * structure the tiled kernel (ws_raster_tiled.cl) consumes: a screen-space tile
 * grid and, per tile, the list of triangle indices whose screen-space bounding
 * box overlaps that tile.
 *
 * The bin is a conservative filter only. A triangle whose bounding box overlaps
 * a tile is kept; the kernel still does the exact edge test per pixel. This
 * makes the tiled kernel bit-identical to the untiled one (same winning write
 * order, same interpolation), which is the whole point of ADR-001's CPU/GPU
 * semantic comparison.
 */
#ifndef WS_GPU_TILEBIN_H
#define WS_GPU_TILEBIN_H

#include <stdint.h>

#include "../core/ws_result.h"

/* A built tile bin. `tri_index` holds, for tile 0 then tile 1 then ..., the
 * triangle indices that may cover that tile, each tile's slice in ascending
 * triangle order. `tile_offset` has tiles_x*tiles_y + 1 entries; tile t owns
 * [tile_offset[t], tile_offset[t + 1]). */
typedef struct ws_tilebin {
    uint32_t tiles_x;
    uint32_t tiles_y;
    uint32_t tile_size;
    uint32_t *tri_index;   /* owned */
    uint32_t *tile_offset; /* owned, length tiles_x*tiles_y + 1 */
    uint64_t total_refs;   /* sum of per-tile list lengths (bin density) */
} ws_tilebin;

/* Build the tile bin for a triangle batch in screen space.
 *
 * vx, vy    screen-space vertex positions in pixels; triangle t occupies
 *           vx/vy[t*3 .. t*3+2] (same layout as ws_opencl_raster).
 * count     number of triangles.
 * width     framebuffer width in pixels (>= 1).
 * height    framebuffer height in pixels (>= 1).
 * tile_size tile edge in pixels; 0 selects a default (16).
 *
 * Returns WS_OK and fills *out on success; the caller must free it with
 * ws_tilebin_free. A triangle whose screen bounding box does not intersect the
 * image is still listed in every tile its clamped box touches only when the
 * box intersects; fully off-screen triangles are dropped. Returns
 * WS_ERR_NULL_POINTER / WS_ERR_INVALID_ARGUMENT on bad input and
 * WS_ERR_OUT_OF_MEMORY when allocation fails.
 */
ws_result ws_tilebin_build(ws_tilebin *out,
                           const float *vx, const float *vy, uint32_t count,
                           uint32_t width, uint32_t height, uint32_t tile_size);

void ws_tilebin_free(ws_tilebin *bin);

#endif /* WS_GPU_TILEBIN_H */
