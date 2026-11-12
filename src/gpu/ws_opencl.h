/* Worldseed Engine — OpenCL 1.2 host wrapper (P10).
 *
 * Loads the system OpenCL ICD at runtime (no bundled runtime, ADR-001), selects
 * a device (GPU preferred, CPU OpenCL device accepted), builds the OpenCL C 1.2
 * raster kernel and runs it. Every API entry point is checked; a device that
 * disappears mid-run sets the "device lost" flag so the caller can fall back to
 * the CPU renderer without corrupting world state.
 *
 * Only the OpenCL 1.2 C API subset used here is declared locally; the shared
 * core has no OpenCL header dependency.
 */
#ifndef WS_GPU_OPENCL_H
#define WS_GPU_OPENCL_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"
#include "../platform/ws_opencl_probe.h"

typedef struct ws_opencl ws_opencl;

/* Try to create an OpenCL compute backend. Returns WS_OK with *out set when a
 * device was found and the kernel built; WS_ERR_UNSUPPORTED (with *out = NULL)
 * when there is no usable device. Absence of a device is not an error. */
ws_result ws_opencl_create(ws_opencl **out);

void ws_opencl_destroy(ws_opencl *cl);

/* 1 when the backend is usable (created and not lost). */
int ws_opencl_available(const ws_opencl *cl);

/* 1 when a runtime OpenCL error marked the device as lost and the caller must
 * fall back to CPU. */
int ws_opencl_device_lost(const ws_opencl *cl);

/* Device summary captured at create time. */
const ws_opencl_info *ws_opencl_info_of(const ws_opencl *cl);

/* Flattened triangle batch for the raster kernel: triangle t occupies
 * vx/vy/vz[t*3 .. t*3+2] and color[t*9 .. t*9+8] (RGB per vertex). */
ws_result ws_opencl_raster(ws_opencl *cl,
                           const float *vx, const float *vy, const float *vz,
                           const uint8_t *color, uint32_t triangle_count,
                           uint32_t width, uint32_t height,
                           uint8_t *pixels, float *depth);

/* Tile-binned variant of ws_opencl_raster (P12). Same inputs and the same
 * bit-identical output image, but the host builds a screen-space tile bin
 * (ws_tilebin) and each pixel work-item only visits its tile's triangle list.
 * This removes the pixel x triangle blow-up of the reference kernel so the path
 * scales to a full scene. `tile_size` is the bin tile edge in pixels (0 = the
 * default). Returns the same codes as ws_opencl_raster. */
ws_result ws_opencl_raster_tiled(ws_opencl *cl,
                                 const float *vx, const float *vy, const float *vz,
                                 const uint8_t *color, uint32_t triangle_count,
                                 uint32_t width, uint32_t height, uint32_t tile_size,
                                 uint8_t *pixels, float *depth);

/* Test-only: simulate a device loss by dropping the live handles and marking
 * the backend lost, so recovery/fallback behaviour can be exercised without a
 * physical GPU removal. No-op when cl is NULL. */
void ws_opencl_simulate_device_loss(ws_opencl *cl);

/* --- Full-scene GPU path (P12) -------------------------------------------
 *
 * The raster kernels above are flat-shaded reference kernels. This entry point
 * runs the whole product scene pipeline (interpolated world position/normal/
 * albedo, Lambert + hemisphere ambient + specular, optional shadow map, fog,
 * sky, encode) on the device. The CPU renderer stays the bit-identical golden
 * reference; this path is the GPU accelerator and is compared to the CPU frame
 * within a documented per-channel tolerance (ADR-001).
 *
 * The caller (ws_render_scene_gpu) marshals the scene into the flat arrays
 * below using the same transform/clip/screen arithmetic as the CPU renderer.
 */
typedef struct ws_gpu_scene_input {
    /* Per-triangle vertex data: 39 floats = 3 vertices x 13 floats
     * (screen x, screen y, ndc z, inv_w, world xyz, normal xyz, albedo rgb).
     * The array is padded with one extra trailing float so the last vertex's
     * blue channel read stays in bounds. */
    const float *verts;
    const uint32_t *tri_obj;   /* object index per triangle */
    const float *obj_mat;      /* 4 floats per object: shininess, mat_diffuse,
                                  metallic, specular */
    uint32_t triangle_count;
    uint32_t object_count;
    const float *consts;       /* scene constants; layout in ws_render.c */
    uint32_t const_count;      /* number of floats in `consts` */
    const float *light_vp;     /* 16 floats; ignored when shadow_depth is NULL */
    const float *shadow_depth; /* shadow_size*shadow_size, or NULL */
    uint32_t shadow_size;
    uint32_t tile_size;        /* screen tile edge in pixels; 0 = default */
} ws_gpu_scene_input;

/* Run the full-scene kernel. `pixels` is width*height*4 RGBA8 and `depth` is
 * width*height floats. Returns WS_OK, or an error (the caller then falls back to
 * the CPU renderer). A NULL shadow_depth disables shadow sampling. */
ws_result ws_opencl_render_scene(ws_opencl *cl, const ws_gpu_scene_input *in,
                                 uint32_t width, uint32_t height,
                                 uint8_t *pixels, float *depth);

#endif /* WS_GPU_OPENCL_H */
