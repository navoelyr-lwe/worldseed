/* Worldseed Engine — tile-binned flat-shaded triangle raster kernel (OpenCL C 1.2).
 *
 * This is the ONLY permitted non-ISO-C source in the product (ADR-001): OpenCL C
 * is a C99-based dialect and the single GPU kernel language exception. It uses
 * only the OpenCL C 1.2 core feature set: no OpenCL 2.x/3.x builtins, no
 * sub-groups, no images/samplers, no atomics.
 *
 * Why this kernel exists: the reference kernel (ws_raster.cl) has every pixel
 * work-item walk the whole triangle list, so cost grows as pixels x triangles
 * and does not scale to a full scene. The host builds a screen-space tile grid
 * and, per tile, a list of triangle indices whose bounding box overlaps that
 * tile (src/gpu/ws_tilebin.c). Each work-item then only visits its tile's list.
 *
 * Semantics are defined to be bit-identical to ws_raster.cl, so the two kernels
 * produce the exact same image for the same batch (verified in tests/test_gpu.c
 * and tools/ws_gpu_check.c):
 *   - one work-item per pixel, so every pixel is written by exactly one item
 *     and no atomics or cross-item ordering are needed (deterministic);
 *   - the same edge-function inside test, the same float barycentric colour and
 *     depth interpolation, and the same smaller-depth-wins / equal-depth-no-
 *     overwrite tie rule as ws_raster.cl;
 *   - the per-tile triangle list preserves the batch's ascending triangle order
 *     (the host emits it that way), so the sequence of winning writes at a pixel
 *     is identical.
 *
 * The engine never makes this kernel authoritative for simulation state; it only
 * accelerates rasterisation of the already-decided frame (ADR-001).
 *
 * Keep this file free of double quotes and backslashes: it is embedded verbatim
 * into a C string literal at configure time (see src/CMakeLists.txt).
 */

__kernel void ws_raster_tiled(
    __global const float *vx,
    __global const float *vy,
    __global const float *vz,
    __global const uchar *color,
    __global const uint *tri_index,
    __global const uint *tile_offset,
    const uint tiles_x,
    const uint tile_size,
    const uint width,
    const uint height,
    __global uchar *pixels,
    __global float *depth)
{
    const uint gid = get_global_id(0);
    const uint count = width * height;
    if (gid >= count) {
        return;
    }

    const uint x = gid % width;
    const uint y = gid / width;
    const uint tx = x / tile_size;
    const uint ty = y / tile_size;
    const uint tile_id = ty * tiles_x + tx;

    const float px = (float)x + 0.5f;
    const float py = (float)y + 0.5f;

    float best_z = 1.0e30f;
    uchar best_r = 0;
    uchar best_g = 0;
    uchar best_b = 0;

    const uint begin = tile_offset[tile_id];
    const uint end = tile_offset[tile_id + 1u];
    for (uint li = begin; li < end; ++li) {
        const uint t = tri_index[li];
        const uint b = t * 3u;
        const float ax = vx[b + 0u];
        const float ay = vy[b + 0u];
        const float bx = vx[b + 1u];
        const float by = vy[b + 1u];
        const float cx = vx[b + 2u];
        const float cy = vy[b + 2u];

        const float area = (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
        if (area == 0.0f) {
            continue;
        }

        const float w0 = (px - bx) * (cy - by) - (py - by) * (cx - bx);
        const float w1 = (px - cx) * (ay - cy) - (py - cy) * (ax - cx);
        const float w2 = (px - ax) * (by - ay) - (py - ay) * (bx - ax);

        int inside;
        if (area > 0.0f) {
            inside = (w0 >= 0.0f) && (w1 >= 0.0f) && (w2 >= 0.0f);
        } else {
            inside = (w0 <= 0.0f) && (w1 <= 0.0f) && (w2 <= 0.0f);
        }
        if (!inside) {
            continue;
        }

        const float inv_area = 1.0f / area;
        const float l0 = w0 * inv_area;
        const float l1 = w1 * inv_area;
        const float l2 = w2 * inv_area;

        const float z = l0 * vz[b + 0u] + l1 * vz[b + 1u] + l2 * vz[b + 2u];
        if (z >= best_z) {
            continue;
        }
        best_z = z;

        const uint c = t * 9u;
        const float r = l0 * (float)color[c + 0u] + l1 * (float)color[c + 3u] + l2 * (float)color[c + 6u];
        const float g = l0 * (float)color[c + 1u] + l1 * (float)color[c + 4u] + l2 * (float)color[c + 7u];
        const float bl = l0 * (float)color[c + 2u] + l1 * (float)color[c + 5u] + l2 * (float)color[c + 8u];

        best_r = (uchar)(r <= 0.0f ? 0.0f : (r >= 255.0f ? 255.0f : r));
        best_g = (uchar)(g <= 0.0f ? 0.0f : (g >= 255.0f ? 255.0f : g));
        best_b = (uchar)(bl <= 0.0f ? 0.0f : (bl >= 255.0f ? 255.0f : bl));
    }

    const uint p = gid * 4u;
    pixels[p + 0u] = best_r;
    pixels[p + 1u] = best_g;
    pixels[p + 2u] = best_b;
    pixels[p + 3u] = 255u;
    depth[gid] = best_z;
}
