/* Worldseed Engine — minimal tiled CPU rasterizer (P2 feasibility prototype).
 *
 * Scope: prove the tiled CPU raster loop, depth test and per-pixel shading on
 * the reference path. It is deliberately small. It is NOT the final renderer
 * and makes no quality or performance claim. Golden-scene quality gates come
 * with the full renderer in P2.
 */
#ifndef WS_RENDER_RASTER_H
#define WS_RENDER_RASTER_H

#include "ws_framebuffer.h"

typedef struct ws_raster_tile_stats {
    uint64_t tiles_visited;
    uint64_t triangles_drawn;
    uint64_t pixels_shaded;
} ws_raster_tile_stats;

/* Draw a Gouraud-shaded triangle with a depth buffer.
 *
 * fb        target colour buffer (RGBA8)
 * depth     depth buffer, width*height floats, smaller wins; may be NULL
 * vx, vy    screen-space vertex positions in pixels
 * vz        per-vertex depth (smaller is nearer)
 * color     flat per-vertex RGB colour: 9 bytes, vertex i at color[i*3 + c]
 *           (c = 0 red, 1 green, 2 blue), values 0..255
 * tile      tile edge in pixels (e.g. 32); 0 means a single full-image tile
 * stats     optional counters; may be NULL
 */
void ws_raster_triangle(ws_framebuffer *fb, float *depth,
                        const float vx[3], const float vy[3], const float vz[3],
                        const unsigned char *color,
                        unsigned tile, ws_raster_tile_stats *stats);

#endif /* WS_RENDER_RASTER_H */
