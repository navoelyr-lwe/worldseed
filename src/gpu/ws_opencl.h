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

#endif /* WS_GPU_OPENCL_H */
