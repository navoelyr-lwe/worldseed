/* Worldseed Engine — procedural terrain regions. */
#include "ws_terrain.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "../core/ws_math.h"
#include "../core/ws_noise.h"

#define TERRAIN_CONTINENT_SALT 0x1111111111111111ULL
#define TERRAIN_HILL_SALT 0x2222222222222222ULL
#define TERRAIN_MOUNTAIN_SALT 0x3333333333333333ULL
#define TERRAIN_MASK_SALT 0x4444444444444444ULL

ws_terrain_config ws_terrain_config_default(uint64_t seed)
{
    ws_terrain_config c;
    c.seed = seed;
    c.version = WS_TERRAIN_VERSION;
    c.cell_size = 8.0;
    c.region_cells = 64u;
    c.base_height = 16.0;
    c.hill_amplitude = 42.0;
    c.mountain_amplitude = 120.0;
    c.water_level = 6.0;
    return c;
}

static double smoothstep(double edge0, double edge1, double x)
{
    double t = ws_clamp_double((x - edge0) / (edge1 - edge0), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

double ws_terrain_height_at(const ws_terrain_config *config, double wx, double wy)
{
    if (config == NULL) {
        return 0.0;
    }
    uint64_t seed = config->seed;

    double continent = ws_noise_fbm2(seed ^ TERRAIN_CONTINENT_SALT, wx / 1200.0, wy / 1200.0,
                                     5, 2.0, 0.5);
    double h = config->base_height + (continent - 0.5) * 2.0 * config->hill_amplitude;

    double hills = ws_noise_fbm2(seed ^ TERRAIN_HILL_SALT, wx / 130.0, wy / 130.0,
                                 5, 2.0, 0.5);
    h += (hills - 0.5) * config->hill_amplitude;

    double mask = ws_noise_fbm2(seed ^ TERRAIN_MASK_SALT, wx / 900.0, wy / 900.0,
                                3, 2.0, 0.5);
    mask = smoothstep(0.55, 0.85, mask);
    if (mask > 0.0) {
        double ridge = ws_noise_ridged2(seed ^ TERRAIN_MOUNTAIN_SALT, wx / 240.0, wy / 240.0,
                                        5, 2.0, 0.5);
        h += mask * ridge * config->mountain_amplitude;
    }
    return h;
}

static uint8_t classify_surface(const ws_terrain_config *config, double height, double slope)
{
    if (slope > 0.62) {
        return WS_SURFACE_ROCK;
    }
    if (height < config->water_level + 1.5) {
        return WS_SURFACE_SAND;
    }
    if (height > config->base_height + config->hill_amplitude * 1.4) {
        return WS_SURFACE_ROCK;
    }
    if (slope > 0.34) {
        return WS_SURFACE_SOIL;
    }
    return WS_SURFACE_GRASS;
}

ws_result ws_terrain_create(ws_terrain *terrain, const ws_terrain_config *config,
                            int64_t rx, int64_t ry)
{
    if (terrain == NULL || config == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (config->region_cells < 2u || config->cell_size <= 0.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(terrain, 0, sizeof(*terrain));
    terrain->config = *config;
    terrain->region_x = rx;
    terrain->region_y = ry;
    terrain->cells = config->region_cells;
    terrain->origin_x = (double)rx * config->cell_size * (double)config->region_cells;
    terrain->origin_y = (double)ry * config->cell_size * (double)config->region_cells;

    size_t nodes = (size_t)config->region_cells + 1u;
    size_t node_count = 0;
    size_t cell_count = 0;
    if (!ws_mul_size_checked(nodes, nodes, &node_count) ||
        !ws_mul_size_checked((size_t)config->region_cells, (size_t)config->region_cells, &cell_count)) {
        return WS_ERR_OVERFLOW;
    }

    size_t height_bytes = 0;
    if (!ws_mul_size_checked(node_count, sizeof(double), &height_bytes)) {
        return WS_ERR_OVERFLOW;
    }
    terrain->heights = (double *)malloc(height_bytes);
    terrain->slope = (double *)malloc(cell_count * sizeof(double));
    terrain->surface = (uint8_t *)malloc(cell_count);
    if (terrain->heights == NULL || terrain->slope == NULL || terrain->surface == NULL) {
        ws_terrain_free(terrain);
        return WS_ERR_OUT_OF_MEMORY;
    }

    for (uint32_t iy = 0; iy <= config->region_cells; ++iy) {
        for (uint32_t ix = 0; ix <= config->region_cells; ++ix) {
            double wx = terrain->origin_x + (double)ix * config->cell_size;
            double wy = terrain->origin_y + (double)iy * config->cell_size;
            terrain->heights[(size_t)iy * nodes + ix] = ws_terrain_height_at(config, wx, wy);
        }
    }

    double inv_cell = 1.0 / config->cell_size;
    for (uint32_t cy = 0; cy < config->region_cells; ++cy) {
        for (uint32_t cx = 0; cx < config->region_cells; ++cx) {
            double hx = (terrain->heights[(size_t)cy * nodes + (cx + 1u)] -
                         terrain->heights[(size_t)cy * nodes + cx]) * inv_cell;
            double hy = (terrain->heights[(size_t)(cy + 1u) * nodes + cx] -
                         terrain->heights[(size_t)cy * nodes + cx]) * inv_cell;
            double slope = sqrt(hx * hx + hy * hy);
            size_t ci = (size_t)cy * config->region_cells + cx;
            terrain->slope[ci] = slope;
            double centre_h = 0.25 * (terrain->heights[(size_t)cy * nodes + cx] +
                                      terrain->heights[(size_t)cy * nodes + cx + 1u] +
                                      terrain->heights[(size_t)(cy + 1u) * nodes + cx] +
                                      terrain->heights[(size_t)(cy + 1u) * nodes + cx + 1u]);
            terrain->surface[ci] = classify_surface(config, centre_h, slope);
        }
    }
    return WS_OK;
}

void ws_terrain_free(ws_terrain *terrain)
{
    if (terrain == NULL) {
        return;
    }
    free(terrain->heights);
    free(terrain->slope);
    free(terrain->surface);
    terrain->heights = NULL;
    terrain->slope = NULL;
    terrain->surface = NULL;
}

double ws_terrain_node(const ws_terrain *terrain, uint32_t ix, uint32_t iy)
{
    if (terrain == NULL || terrain->heights == NULL ||
        ix > terrain->cells || iy > terrain->cells) {
        return 0.0;
    }
    size_t nodes = (size_t)terrain->cells + 1u;
    return terrain->heights[(size_t)iy * nodes + ix];
}

double ws_terrain_height_bilinear(const ws_terrain *terrain, double wx, double wy)
{
    if (terrain == NULL || terrain->heights == NULL) {
        return 0.0;
    }
    double lx = (wx - terrain->origin_x) / terrain->config.cell_size;
    double ly = (wy - terrain->origin_y) / terrain->config.cell_size;
    double cx = ws_clamp_double(lx, 0.0, (double)terrain->cells);
    double cy = ws_clamp_double(ly, 0.0, (double)terrain->cells);
    uint32_t ix = (uint32_t)cx;
    uint32_t iy = (uint32_t)cy;
    if (ix >= terrain->cells) {
        ix = terrain->cells - 1u;
    }
    if (iy >= terrain->cells) {
        iy = terrain->cells - 1u;
    }
    double fx = cx - (double)ix;
    double fy = cy - (double)iy;
    double h00 = ws_terrain_node(terrain, ix, iy);
    double h10 = ws_terrain_node(terrain, ix + 1u, iy);
    double h01 = ws_terrain_node(terrain, ix, iy + 1u);
    double h11 = ws_terrain_node(terrain, ix + 1u, iy + 1u);
    double top = ws_lerp(h00, h10, fx);
    double bot = ws_lerp(h01, h11, fx);
    return ws_lerp(top, bot, fy);
}

uint8_t ws_terrain_cell_surface(const ws_terrain *terrain, uint32_t cx, uint32_t cy)
{
    if (terrain == NULL || terrain->surface == NULL ||
        cx >= terrain->cells || cy >= terrain->cells) {
        return WS_SURFACE_GRASS;
    }
    return terrain->surface[(size_t)cy * terrain->cells + cx];
}

double ws_terrain_cell_slope(const ws_terrain *terrain, uint32_t cx, uint32_t cy)
{
    if (terrain == NULL || terrain->slope == NULL ||
        cx >= terrain->cells || cy >= terrain->cells) {
        return 0.0;
    }
    return terrain->slope[(size_t)cy * terrain->cells + cx];
}

const char *ws_surface_class_name(uint8_t surface)
{
    switch (surface) {
    case WS_SURFACE_GRASS: return "grass";
    case WS_SURFACE_SOIL: return "soil";
    case WS_SURFACE_ROCK: return "rock";
    case WS_SURFACE_SAND: return "sand";
    default: return "unknown";
    }
}
