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

/* Which renderer produced a frame (ws_render_stats.path). */
enum {
    WS_RENDER_PATH_CPU = 0,
    WS_RENDER_PATH_GPU = 1
};

typedef struct ws_render_stats {
    uint64_t triangles_submitted;
    uint64_t triangles_clipped; /* dropped by near-plane clipping */
    uint64_t triangles_drawn;
    uint64_t pixels_shaded;
    uint64_t objects_culled; /* objects skipped by the camera frustum test */
    int path; /* 0 = CPU reference, 1 = OpenCL accelerator (see ws_render_scene_gpu) */
} ws_render_stats;

/* Render `scene` into `fb`. `depth` is an optional width*height float buffer
 * (smaller wins); pass NULL to disable depth testing. `tile` is the tile edge in
 * pixels (0 = single tile). `stats` is optional. Returns WS_OK or an error when
 * arguments are invalid. */
ws_result ws_render_scene(const ws_scene *scene, ws_framebuffer *fb,
                          float *depth, unsigned tile, ws_render_stats *stats);

/* Render `scene` with the OpenCL full-scene accelerator when a device is present,
 * falling back to the bit-identical CPU reference renderer otherwise. The CPU
 * path stays the golden reference; the GPU frame matches it within the documented
 * per-channel tolerance (ADR-001). `stats->path` reports which path ran. */
ws_result ws_render_scene_gpu(const ws_scene *scene, ws_framebuffer *fb,
                              float *depth, unsigned tile, ws_render_stats *stats);

#endif /* WS_RENDER_RENDER_H */
