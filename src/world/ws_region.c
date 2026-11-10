/* Worldseed Engine — region grid geometry (P8). */
#include "ws_region.h"

#include <math.h>

#include "../core/ws_hash.h"

#define WS_REGION_DEFAULT_CELLS 64u
#define WS_REGION_DEFAULT_CELL_SIZE 8.0

ws_region_config ws_region_config_default(uint64_t seed)
{
    ws_region_config c;
    c.seed = seed;
    c.cells = WS_REGION_DEFAULT_CELLS;
    c.cell_size = WS_REGION_DEFAULT_CELL_SIZE;
    return c;
}

double ws_region_extent(const ws_region_config *config)
{
    if (config == NULL) {
        return 0.0;
    }
    return (double)config->cells * config->cell_size;
}

ws_region_coord ws_region_coord_make(int64_t x, int64_t y)
{
    ws_region_coord c;
    c.x = x;
    c.y = y;
    return c;
}

int ws_region_coord_equal(ws_region_coord a, ws_region_coord b)
{
    return a.x == b.x && a.y == b.y;
}

ws_region_pos ws_region_from_world(const ws_region_config *config, double wx, double wy)
{
    ws_region_pos p;
    p.region = ws_region_coord_make(0, 0);
    p.local_x = 0.0;
    p.local_y = 0.0;
    if (config == NULL) {
        return p;
    }
    double extent = ws_region_extent(config);
    double fx = floor(wx / extent);
    double fy = floor(wy / extent);
    p.region = ws_region_coord_make((int64_t)fx, (int64_t)fy);
    p.local_x = wx - fx * extent;
    p.local_y = wy - fy * extent;
    /* Guard against floating-point edge cases landing exactly on the boundary. */
    if (p.local_x < 0.0) { p.local_x = 0.0; }
    if (p.local_x >= extent) { p.local_x = extent; }
    if (p.local_y < 0.0) { p.local_y = 0.0; }
    if (p.local_y >= extent) { p.local_y = extent; }
    return p;
}

void ws_region_to_world(const ws_region_config *config, ws_region_coord region,
                        double local_x, double local_y, double *out_wx, double *out_wy)
{
    if (config == NULL) {
        if (out_wx != NULL) { *out_wx = 0.0; }
        if (out_wy != NULL) { *out_wy = 0.0; }
        return;
    }
    double extent = ws_region_extent(config);
    if (out_wx != NULL) { *out_wx = (double)region.x * extent + local_x; }
    if (out_wy != NULL) { *out_wy = (double)region.y * extent + local_y; }
}

void ws_region_center_world(const ws_region_config *config, ws_region_coord region,
                            double *out_wx, double *out_wy)
{
    double half = ws_region_extent(config) * 0.5;
    ws_region_to_world(config, region, half, half, out_wx, out_wy);
}

int ws_region_adjacent(ws_region_coord a, ws_region_coord b)
{
    int64_t dx = a.x - b.x;
    int64_t dy = a.y - b.y;
    if (dx < 0) { dx = -dx; }
    if (dy < 0) { dy = -dy; }
    return (dx + dy) == 1;
}

int64_t ws_region_distance(ws_region_coord a, ws_region_coord b)
{
    int64_t dx = a.x - b.x;
    int64_t dy = a.y - b.y;
    if (dx < 0) { dx = -dx; }
    if (dy < 0) { dy = -dy; }
    return dx > dy ? dx : dy;
}

uint64_t ws_region_hash(const ws_region_config *config, ws_region_coord region)
{
    uint64_t seed = (config != NULL) ? config->seed : 0u;
    uint8_t buf[24];
    for (int i = 0; i < 8; ++i) {
        buf[i] = (uint8_t)(seed >> (8 * i));
    }
    uint64_t ux = (uint64_t)region.x;
    uint64_t uy = (uint64_t)region.y;
    for (int i = 0; i < 8; ++i) {
        buf[8 + i] = (uint8_t)(ux >> (8 * i));
        buf[16 + i] = (uint8_t)(uy >> (8 * i));
    }
    uint8_t digest[WS_SHA256_DIGEST_SIZE];
    ws_sha256_buffer(buf, sizeof(buf), digest);
    uint64_t out = 0u;
    for (int i = 0; i < 8; ++i) {
        out = (out << 8) | (uint64_t)digest[i];
    }
    return out;
}
