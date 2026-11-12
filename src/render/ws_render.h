/* Worldseed Engine — reference CPU scene renderer.
 *
 * Renders a ws_scene (camera + light + instanced meshes) into an RGBA8
 * framebuffer with a float depth buffer. Pipeline: model/view/projection
 * transform, near-plane clipping, perspective divide, viewport map, tiled
 * scanline rasterisation with perspective-correct attribute interpolation and
 * per-pixel Lambert shading. This is the mandatory correctness reference and
 * CPU fallback path; it is not the final high-quality renderer.
 */
#ifndef WS_RENDER_RENDER_H
#define WS_RENDER_RENDER_H

#include "../core/ws_result.h"
#include "ws_framebuffer.h"
#include "ws_scene.h"

typedef struct ws_render_stats {
    uint64_t triangles_submitted;
    uint64_t triangles_clipped; /* dropped by near-plane clipping */
    uint64_t triangles_drawn;
    uint64_t pixels_shaded;
    uint64_t objects_culled; /* objects skipped by the camera frustum test */
} ws_render_stats;

/* Render `scene` into `fb`. `depth` is an optional width*height float buffer
 * (smaller wins); pass NULL to disable depth testing. `tile` is the tile edge in
 * pixels (0 = single tile). `stats` is optional. Returns WS_OK or an error when
 * arguments are invalid. */
ws_result ws_render_scene(const ws_scene *scene, ws_framebuffer *fb,
                          float *depth, unsigned tile, ws_render_stats *stats);

#endif /* WS_RENDER_RENDER_H */
