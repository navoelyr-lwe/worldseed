/* Worldseed Engine — deterministic plant placement. */
#include "ws_vegetation.h"

#include <stdlib.h>
#include <string.h>

#include "../core/ws_noise.h"

/* Placement probability per habitat and plant kind. */
static double density(uint8_t habitat, uint8_t kind)
{
    switch (habitat) {
    case WS_HABITAT_FOREST:
        switch (kind) {
        case WS_PLANT_TREE: return 0.45;
        case WS_PLANT_SHRUB: return 0.18;
        default: return 0.10;
        }
    case WS_HABITAT_MEADOW:
        switch (kind) {
        case WS_PLANT_TREE: return 0.02;
        case WS_PLANT_SHRUB: return 0.05;
        default: return 0.55;
        }
    case WS_HABITAT_ROCKY:
        return (kind == WS_PLANT_GRASS) ? 0.08 : 0.0;
    default: /* freshwater */
        return 0.0;
    }
}

static float plant_height(uint8_t kind, double noise)
{
    switch (kind) {
    case WS_PLANT_TREE: return (float)(6.0 + noise * 8.0);
    case WS_PLANT_SHRUB: return (float)(0.8 + noise * 1.2);
    default: return (float)(0.15 + noise * 0.35);
    }
}

ws_result ws_vegetation_create(ws_vegetation *veg, const ws_terrain *terrain,
                               const ws_habitat_map *habitat)
{
    if (veg == NULL || terrain == NULL || habitat == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (terrain->cells == 0u || habitat->cells != terrain->cells) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(veg, 0, sizeof(*veg));
    size_t cell_count = (size_t)terrain->cells * (size_t)terrain->cells;
    /* Worst case: every cell can hold one plant of each kind.
     * calloc so struct padding is deterministic: callers compare/hash the
     * plant array as raw bytes (same reason vertices are zeroed). */
    size_t capacity = cell_count * (size_t)WS_PLANT_KIND_COUNT;
    veg->plants = (ws_plant *)calloc(capacity, sizeof(ws_plant));
    if (veg->plants == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    veg->capacity = (uint32_t)capacity;

    uint32_t n = terrain->cells;
    double cell = terrain->config.cell_size;
    for (uint32_t cy = 0; cy < n; ++cy) {
        for (uint32_t cx = 0; cx < n; ++cx) {
            uint8_t kind_hab = ws_habitat_at(habitat, cx, cy);
            for (uint8_t kind = 0; kind < WS_PLANT_KIND_COUNT; ++kind) {
                double p = density(kind_hab, kind);
                if (p <= 0.0) {
                    continue;
                }
                uint64_t salt = 0xA1B2C3D4E5F60718ULL ^ ((uint64_t)kind * 0x9E3779B97F4A7C15ULL);
                double roll = ws_noise_hash2(terrain->config.seed ^ salt, (int64_t)cx, (int64_t)cy);
                if (roll >= p) {
                    continue;
                }
                double jx = ws_noise_hash2(terrain->config.seed ^ salt ^ 0x11ULL, (int64_t)cx, (int64_t)cy);
                double jy = ws_noise_hash2(terrain->config.seed ^ salt ^ 0x22ULL, (int64_t)cx, (int64_t)cy);
                double wx = terrain->origin_x + ((double)cx + jx) * cell;
                double wy = terrain->origin_y + ((double)cy + jy) * cell;
                double hn = ws_noise_hash2(terrain->config.seed ^ salt ^ 0x33ULL, (int64_t)cx, (int64_t)cy);
                ws_plant *pl = &veg->plants[veg->count++];
                pl->position = ws_vec2_make(wx, wy);
                pl->height = plant_height(kind, hn);
                pl->kind = kind;
                veg->counts[kind] += 1u;
            }
        }
    }
    return WS_OK;
}

void ws_vegetation_free(ws_vegetation *veg)
{
    if (veg == NULL) {
        return;
    }
    free(veg->plants);
    veg->plants = NULL;
    veg->count = 0u;
}

const char *ws_plant_kind_name(uint8_t kind)
{
    switch (kind) {
    case WS_PLANT_GRASS: return "grass";
    case WS_PLANT_SHRUB: return "shrub";
    case WS_PLANT_TREE: return "tree";
    default: return "unknown";
    }
}
