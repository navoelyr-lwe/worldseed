/* Worldseed Engine — screen-space tile binning (P12). See ws_tilebin.h. */
#include "ws_tilebin.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define WS_TILEBIN_DEFAULT_TILE 16u

/* Clamp a float coordinate to the integer pixel range [0, hi]. */
static int32_t clamp_px(float v, int32_t hi)
{
    if (v < 0.0f) {
        return 0;
    }
    if (v > (float)hi) {
        return hi;
    }
    return (int32_t)v;
}

ws_result ws_tilebin_build(ws_tilebin *out,
                           const float *vx, const float *vy, uint32_t count,
                           uint32_t width, uint32_t height, uint32_t tile_size)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(out, 0, sizeof(*out));
    if ((count > 0u && (vx == NULL || vy == NULL)) || width == 0u || height == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (tile_size == 0u) {
        tile_size = WS_TILEBIN_DEFAULT_TILE;
    }

    uint32_t tiles_x = (width + tile_size - 1u) / tile_size;
    uint32_t tiles_y = (height + tile_size - 1u) / tile_size;
    size_t tile_count = (size_t)tiles_x * (size_t)tiles_y;
    if (tile_count == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    uint32_t *offsets = (uint32_t *)calloc(tile_count + 1u, sizeof(uint32_t));
    if (offsets == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }

    /* Pass 1: count how many triangles touch each tile. */
    for (uint32_t t = 0u; t < count; ++t) {
        float ax = vx[t * 3u + 0u], ay = vy[t * 3u + 0u];
        float bx = vx[t * 3u + 1u], by = vy[t * 3u + 1u];
        float cx = vx[t * 3u + 2u], cy = vy[t * 3u + 2u];
        float min_x = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
        float max_x = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
        float min_y = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
        float max_y = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
        if (min_x > (float)(width - 1u) || max_x < 0.0f ||
            min_y > (float)(height - 1u) || max_y < 0.0f) {
            continue; /* fully off-screen */
        }
        int32_t x0 = clamp_px(min_x, (int32_t)width - 1);
        int32_t x1 = clamp_px(max_x, (int32_t)width - 1);
        int32_t y0 = clamp_px(min_y, (int32_t)height - 1);
        int32_t y1 = clamp_px(max_y, (int32_t)height - 1);
        uint32_t tx0 = (uint32_t)x0 / tile_size, tx1 = (uint32_t)x1 / tile_size;
        uint32_t ty0 = (uint32_t)y0 / tile_size, ty1 = (uint32_t)y1 / tile_size;
        for (uint32_t ty = ty0; ty <= ty1; ++ty) {
            for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                offsets[(size_t)ty * tiles_x + tx + 1u] += 1u;
            }
        }
    }

    /* Prefix sum: offsets[t] is now the start of tile t's slice. */
    for (size_t i = 0u; i < tile_count; ++i) {
        offsets[i + 1u] += offsets[i];
    }
    uint64_t total_refs = offsets[tile_count];

    uint32_t *tri_index = NULL;
    if (total_refs > 0u) {
        tri_index = (uint32_t *)malloc((size_t)total_refs * sizeof(uint32_t));
        if (tri_index == NULL) {
            free(offsets);
            return WS_ERR_OUT_OF_MEMORY;
        }
    }

    /* Pass 2: fill each tile's slice in ascending triangle order. A per-tile
     * cursor appended in triangle order keeps every slice sorted, so the kernel
     * visits triangles in the same order as the untiled kernel. */
    uint32_t *cursor = (uint32_t *)calloc(tile_count, sizeof(uint32_t));
    if (cursor == NULL) {
        free(tri_index);
        free(offsets);
        return WS_ERR_OUT_OF_MEMORY;
    }
    for (uint32_t t = 0u; t < count; ++t) {
        float ax = vx[t * 3u + 0u], ay = vy[t * 3u + 0u];
        float bx = vx[t * 3u + 1u], by = vy[t * 3u + 1u];
        float cx = vx[t * 3u + 2u], cy = vy[t * 3u + 2u];
        float min_x = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
        float max_x = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
        float min_y = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
        float max_y = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
        if (min_x > (float)(width - 1u) || max_x < 0.0f ||
            min_y > (float)(height - 1u) || max_y < 0.0f) {
            continue;
        }
        int32_t x0 = clamp_px(min_x, (int32_t)width - 1);
        int32_t x1 = clamp_px(max_x, (int32_t)width - 1);
        int32_t y0 = clamp_px(min_y, (int32_t)height - 1);
        int32_t y1 = clamp_px(max_y, (int32_t)height - 1);
        uint32_t tx0 = (uint32_t)x0 / tile_size, tx1 = (uint32_t)x1 / tile_size;
        uint32_t ty0 = (uint32_t)y0 / tile_size, ty1 = (uint32_t)y1 / tile_size;
        for (uint32_t ty = ty0; ty <= ty1; ++ty) {
            for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                size_t tile = (size_t)ty * tiles_x + tx;
                tri_index[offsets[tile] + cursor[tile]] = t;
                cursor[tile] += 1u;
            }
        }
    }

    free(cursor);
    out->tiles_x = tiles_x;
    out->tiles_y = tiles_y;
    out->tile_size = tile_size;
    out->tri_index = tri_index;
    out->tile_offset = offsets;
    out->total_refs = total_refs;
    return WS_OK;
}

void ws_tilebin_free(ws_tilebin *bin)
{
    if (bin == NULL) {
        return;
    }
    free(bin->tri_index);
    free(bin->tile_offset);
    memset(bin, 0, sizeof(*bin));
}

ws_result ws_tilebin_build_strided(ws_tilebin *out,
                                   const float *tri_verts, uint32_t vert_stride,
                                   uint32_t x_off, uint32_t y_off, uint32_t count,
                                   uint32_t width, uint32_t height, uint32_t tile_size)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    memset(out, 0, sizeof(*out));
    if ((count > 0u && tri_verts == NULL) || width == 0u || height == 0u ||
        vert_stride == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    if (tile_size == 0u) {
        tile_size = WS_TILEBIN_DEFAULT_TILE;
    }

    uint32_t tiles_x = (width + tile_size - 1u) / tile_size;
    uint32_t tiles_y = (height + tile_size - 1u) / tile_size;
    size_t tile_count = (size_t)tiles_x * (size_t)tiles_y;
    if (tile_count == 0u) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    uint32_t *offsets = (uint32_t *)calloc(tile_count + 1u, sizeof(uint32_t));
    if (offsets == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }

    /* Pass 1: count how many triangles touch each tile (stored at tile + 1). */
    for (uint32_t t = 0u; t < count; ++t) {
        const float *tri = tri_verts + (size_t)t * 3u * vert_stride;
        float ax = tri[x_off], ay = tri[y_off];
        float bx = tri[vert_stride + x_off], by = tri[vert_stride + y_off];
        float cx = tri[2u * vert_stride + x_off], cy = tri[2u * vert_stride + y_off];
        float min_x = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
        float max_x = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
        float min_y = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
        float max_y = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
        if (min_x > (float)(width - 1u) || max_x < 0.0f ||
            min_y > (float)(height - 1u) || max_y < 0.0f) {
            continue;
        }
        int32_t x0 = clamp_px(min_x, (int32_t)width - 1);
        int32_t x1 = clamp_px(max_x, (int32_t)width - 1);
        int32_t y0 = clamp_px(min_y, (int32_t)height - 1);
        int32_t y1 = clamp_px(max_y, (int32_t)height - 1);
        uint32_t tx0 = (uint32_t)x0 / tile_size, tx1 = (uint32_t)x1 / tile_size;
        uint32_t ty0 = (uint32_t)y0 / tile_size, ty1 = (uint32_t)y1 / tile_size;
        for (uint32_t ty = ty0; ty <= ty1; ++ty) {
            for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                offsets[(size_t)ty * tiles_x + tx + 1u] += 1u;
            }
        }
    }

    /* Prefix sum: offsets[t] becomes the start of tile t's slice. */
    for (size_t i = 0u; i < tile_count; ++i) {
        offsets[i + 1u] += offsets[i];
    }
    uint64_t total_refs = offsets[tile_count];

    uint32_t *tri_index = NULL;
    if (total_refs > 0u) {
        tri_index = (uint32_t *)malloc((size_t)total_refs * sizeof(uint32_t));
        if (tri_index == NULL) {
            free(offsets);
            return WS_ERR_OUT_OF_MEMORY;
        }
    }
    uint32_t *cursor = (uint32_t *)calloc(tile_count, sizeof(uint32_t));
    if (cursor == NULL) {
        free(tri_index);
        free(offsets);
        return WS_ERR_OUT_OF_MEMORY;
    }

    /* Pass 2: fill each tile's slice in ascending triangle order. */
    for (uint32_t t = 0u; t < count; ++t) {
        const float *tri = tri_verts + (size_t)t * 3u * vert_stride;
        float ax = tri[x_off], ay = tri[y_off];
        float bx = tri[vert_stride + x_off], by = tri[vert_stride + y_off];
        float cx = tri[2u * vert_stride + x_off], cy = tri[2u * vert_stride + y_off];
        float min_x = ax < bx ? (ax < cx ? ax : cx) : (bx < cx ? bx : cx);
        float max_x = ax > bx ? (ax > cx ? ax : cx) : (bx > cx ? bx : cx);
        float min_y = ay < by ? (ay < cy ? ay : cy) : (by < cy ? by : cy);
        float max_y = ay > by ? (ay > cy ? ay : cy) : (by > cy ? by : cy);
        if (min_x > (float)(width - 1u) || max_x < 0.0f ||
            min_y > (float)(height - 1u) || max_y < 0.0f) {
            continue;
        }
        int32_t x0 = clamp_px(min_x, (int32_t)width - 1);
        int32_t x1 = clamp_px(max_x, (int32_t)width - 1);
        int32_t y0 = clamp_px(min_y, (int32_t)height - 1);
        int32_t y1 = clamp_px(max_y, (int32_t)height - 1);
        uint32_t tx0 = (uint32_t)x0 / tile_size, tx1 = (uint32_t)x1 / tile_size;
        uint32_t ty0 = (uint32_t)y0 / tile_size, ty1 = (uint32_t)y1 / tile_size;
        for (uint32_t ty = ty0; ty <= ty1; ++ty) {
            for (uint32_t tx = tx0; tx <= tx1; ++tx) {
                size_t tile = (size_t)ty * tiles_x + tx;
                tri_index[offsets[tile] + cursor[tile]] = t;
                cursor[tile] += 1u;
            }
        }
    }

    free(cursor);
    out->tiles_x = tiles_x;
    out->tiles_y = tiles_y;
    out->tile_size = tile_size;
    out->tri_index = tri_index;
    out->tile_offset = offsets;
    out->total_refs = total_refs;
    return WS_OK;
}
