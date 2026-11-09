/* Worldseed Engine — habitat classification. */
#include "ws_habitat.h"

#include <stdlib.h>
#include <string.h>

#include "../core/ws_noise.h"

#define HABITAT_FOREST_SALT 0x5555555555555555ULL

ws_result ws_habitat_create(ws_habitat_map *map, const ws_terrain *terrain,
                            const ws_hydro *hydro)
{
    if (map == NULL || terrain == NULL || hydro == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (terrain->cells == 0u || terrain->heights == NULL ||
        hydro->cells != terrain->cells || hydro->water_depth == NULL) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(map, 0, sizeof(*map));
    map->cells = terrain->cells;
    size_t count = (size_t)terrain->cells * (size_t)terrain->cells;
    map->kind = (uint8_t *)malloc(count);
    if (map->kind == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }

    uint32_t n = terrain->cells;
    double water_level = terrain->config.water_level;
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            size_t ci = (size_t)cy * n + cx;
            uint8_t surface = ws_terrain_cell_surface(terrain, cx, cy);
            double slope = ws_terrain_cell_slope(terrain, cx, cy);
            double depth = hydro->water_depth[ci];

            uint8_t kind;
            if (hydro->is_river[ci] && depth > 0.0 && terrain->heights[ci] < water_level) {
                kind = WS_HABITAT_FRESHWATER;
            } else if (surface == WS_SURFACE_ROCK || slope > 0.5) {
                kind = WS_HABITAT_ROCKY;
            } else if (hydro->is_river[ci]) {
                kind = WS_HABITAT_FRESHWATER;
            } else {
                /* Forest versus meadow from a slow noise field so the two form
                 * coherent patches rather than per-cell speckle. */
                double wx = terrain->origin_x + ((double)cx + 0.5) * terrain->config.cell_size;
                double wy = terrain->origin_y + ((double)cy + 0.5) * terrain->config.cell_size;
                double f = ws_noise_fbm2(terrain->config.seed ^ HABITAT_FOREST_SALT,
                                         wx / 210.0, wy / 210.0, 3, 2.0, 0.5);
                kind = (f > 0.5) ? WS_HABITAT_FOREST : WS_HABITAT_MEADOW;
            }
            map->kind[ci] = kind;
            map->counts[kind] += 1u;
        }
    }
    return WS_OK;
}

void ws_habitat_free(ws_habitat_map *map)
{
    if (map == NULL) {
        return;
    }
    free(map->kind);
    map->kind = NULL;
}

uint8_t ws_habitat_at(const ws_habitat_map *map, uint32_t cx, uint32_t cy)
{
    if (map == NULL || map->kind == NULL || cx >= map->cells || cy >= map->cells) {
        return WS_HABITAT_MEADOW;
    }
    return map->kind[(size_t)cy * map->cells + cx];
}

double ws_habitat_fraction(const ws_habitat_map *map, uint8_t habitat)
{
    if (map == NULL || habitat >= WS_HABITAT_COUNT) {
        return 0.0;
    }
    double total = (double)map->cells * (double)map->cells;
    if (total <= 0.0) {
        return 0.0;
    }
    return (double)map->counts[habitat] / total;
}

const char *ws_habitat_name(uint8_t habitat)
{
    switch (habitat) {
    case WS_HABITAT_ROCKY: return "rocky";
    case WS_HABITAT_FRESHWATER: return "freshwater";
    case WS_HABITAT_FOREST: return "forest";
    case WS_HABITAT_MEADOW: return "meadow";
    default: return "unknown";
    }
}
