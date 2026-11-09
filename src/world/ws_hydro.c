/* Worldseed Engine — surface water: flow routing, rivers and budget. */
#include "ws_hydro.h"

#include <stdlib.h>
#include <string.h>

#include "../core/ws_math.h"

/* D8 neighbour offsets (dx, dy) in cell space, clockwise from east. */
static const int NEIGHBOR_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static const int NEIGHBOR_DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

ws_hydro_config ws_hydro_config_default(const ws_terrain_config *terrain)
{
    ws_hydro_config c;
    c.water_level = terrain != NULL ? terrain->water_level : 4.0;
    c.precipitation = 1.0e-6; /* ~1 mm over the region per second at 8 m cells */
    c.evaporation = 2.0e-6;
    c.river_threshold = 40u;
    return c;
}

/* Approximate cell-centre height, matching the terrain surface classifier. */
static double cell_height(const ws_terrain *t, uint32_t cx, uint32_t cy)
{
    size_t nodes = (size_t)t->cells + 1u;
    double h = 0.25 * (t->heights[(size_t)cy * nodes + cx] +
                       t->heights[(size_t)cy * nodes + cx + 1u] +
                       t->heights[(size_t)(cy + 1u) * nodes + cx] +
                       t->heights[(size_t)(cy + 1u) * nodes + cx + 1u]);
    return h;
}

typedef struct sortable_cell {
    double height;
    size_t index;
} sortable_cell;

/* Descending height; ties break by ascending index so the order is total and
 * therefore identical on every platform regardless of sort stability. */
static int compare_cell_desc(const void *a, const void *b)
{
    const sortable_cell *ca = (const sortable_cell *)a;
    const sortable_cell *cb = (const sortable_cell *)b;
    if (ca->height > cb->height) {
        return -1;
    }
    if (ca->height < cb->height) {
        return 1;
    }
    if (ca->index < cb->index) {
        return -1;
    }
    if (ca->index > cb->index) {
        return 1;
    }
    return 0;
}

ws_result ws_hydro_create(ws_hydro *hydro, const ws_hydro_config *config,
                          const ws_terrain *terrain)
{
    if (hydro == NULL || config == NULL || terrain == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (terrain->cells == 0u || terrain->heights == NULL) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(hydro, 0, sizeof(*hydro));
    hydro->config = *config;
    hydro->cells = terrain->cells;
    hydro->cell_area = terrain->config.cell_size * terrain->config.cell_size;

    size_t count = (size_t)terrain->cells * (size_t)terrain->cells;
    hydro->water_depth = (double *)calloc(count, sizeof(double));
    hydro->flow_accum = (double *)calloc(count, sizeof(double));
    hydro->flow_dir = (int8_t *)malloc(count);
    hydro->is_river = (uint8_t *)calloc(count, 1u);
    if (hydro->water_depth == NULL || hydro->flow_accum == NULL ||
        hydro->flow_dir == NULL || hydro->is_river == NULL) {
        ws_hydro_free(hydro);
        return WS_ERR_OUT_OF_MEMORY;
    }

    uint32_t n = terrain->cells;
    /* Steepest-descent routing: each interior cell drains to its lowest
     * neighbour; boundary cells and local minima become sinks. */
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            size_t ci = (size_t)cy * n + cx;
            double h = cell_height(terrain, cx, cy);
            int8_t best = -1;
            double best_h = h;
            for (int k = 0; k < 8; ++k) {
                int nx = (int)cx + NEIGHBOR_DX[k];
                int ny = (int)cy + NEIGHBOR_DY[k];
                if (nx < 0 || ny < 0 || nx >= (int)n || ny >= (int)n) {
                    continue;
                }
                double nh = cell_height(terrain, (uint32_t)nx, (uint32_t)ny);
                if (nh < best_h) {
                    best_h = nh;
                    best = (int8_t)k;
                }
            }
            hydro->flow_dir[ci] = best;
        }
    }

    /* Flow accumulation in descending height order (higher cells feed lower). */
    sortable_cell *order = (sortable_cell *)malloc(count * sizeof(sortable_cell));
    if (order == NULL) {
        ws_hydro_free(hydro);
        return WS_ERR_OUT_OF_MEMORY;
    }
    for (size_t i = 0; i < count; ++i) {
        order[i].height = cell_height(terrain, (uint32_t)(i % n), (uint32_t)(i / n));
        order[i].index = i;
    }
    qsort(order, count, sizeof(sortable_cell), compare_cell_desc);
    for (size_t i = 0; i < count; ++i) {
        hydro->flow_accum[i] = 1.0;
    }
    for (size_t i = 0; i < count; ++i) {
        size_t ci = order[i].index;
        int8_t d = hydro->flow_dir[ci];
        if (d < 0) {
            continue;
        }
        uint32_t cx = (uint32_t)(ci % n);
        uint32_t cy = (uint32_t)(ci / n);
        int nx = (int)cx + NEIGHBOR_DX[d];
        int ny = (int)cy + NEIGHBOR_DY[d];
        if (nx < 0 || ny < 0 || nx >= (int)n || ny >= (int)n) {
            continue;
        }
        size_t ni = (size_t)ny * n + (uint32_t)nx;
        hydro->flow_accum[ni] += hydro->flow_accum[ci];
    }

    hydro->river_cells = 0u;
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            size_t ci = (size_t)cy * n + cx;
            double h = cell_height(terrain, cx, cy);
            if (h < config->water_level) {
                hydro->water_depth[ci] = config->water_level - h;
                hydro->is_river[ci] = 1u;
            } else if (hydro->flow_accum[ci] >= (double)config->river_threshold) {
                hydro->water_depth[ci] = 0.6; /* nominal channel depth */
                hydro->is_river[ci] = 1u;
            }
            if (hydro->is_river[ci]) {
                hydro->river_cells += 1u;
            }
        }
    }

    free(order);

    hydro->total_water = 0.0;
    for (size_t i = 0; i < count; ++i) {
        hydro->total_water += hydro->water_depth[i] * hydro->cell_area;
    }
    return WS_OK;
}

void ws_hydro_free(ws_hydro *hydro)
{
    if (hydro == NULL) {
        return;
    }
    free(hydro->water_depth);
    free(hydro->flow_accum);
    free(hydro->flow_dir);
    free(hydro->is_river);
    hydro->water_depth = NULL;
    hydro->flow_accum = NULL;
    hydro->flow_dir = NULL;
    hydro->is_river = NULL;
}

double ws_hydro_budget_step(ws_hydro *hydro, double dt)
{
    if (hydro == NULL || hydro->water_depth == NULL || dt <= 0.0) {
        return 0.0;
    }
    size_t count = (size_t)hydro->cells * (size_t)hydro->cells;
    double add = hydro->config.precipitation * dt;
    double loss_factor = hydro->config.evaporation * dt;
    double before = hydro->total_water;
    double total = 0.0;
    for (size_t i = 0; i < count; ++i) {
        double d = hydro->water_depth[i] + add;
        d -= d * loss_factor;
        if (d < 0.0) {
            d = 0.0;
        }
        hydro->water_depth[i] = d;
        total += d * hydro->cell_area;
    }
    hydro->total_water = total;
    return total - before;
}

double ws_hydro_surface_height(const ws_hydro *hydro, const ws_terrain *terrain,
                               uint32_t cx, uint32_t cy)
{
    if (hydro == NULL || terrain == NULL || cx >= hydro->cells || cy >= hydro->cells) {
        return 0.0;
    }
    size_t ci = (size_t)cy * hydro->cells + cx;
    return cell_height(terrain, cx, cy) + hydro->water_depth[ci];
}
