/* Worldseed Engine — surface water: flow routing, rivers and budget.
 *
 * A deliberately simple, documented model for the first release (docs/04): the
 * terrain defines a flow network with D8 steepest descent, flow accumulates
 * downstream, and cells whose upstream area passes a threshold become rivers.
 * Cells below the water level are still water (sea/lake). Water is tracked as a
 * depth field so the visual surface and the simulated state share one source of
 * truth.
 *
 * A full Navier-Stokes or shallow-water solve is explicitly out of scope here;
 * validity limits are recorded in the model card. The budget step conserves
 * mass by construction: total water changes only by precipitation minus
 * evaporation, which the tests assert.
 */
#ifndef WS_WORLD_HYDRO_H
#define WS_WORLD_HYDRO_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_terrain.h"

#define WS_HYDRO_VERSION 1u

typedef struct ws_hydro_config {
    double water_level;       /* absolute metres; cells below are still water */
    double precipitation;     /* metres added per second over the region */
    double evaporation;       /* fraction of standing water lost per second */
    uint32_t river_threshold; /* upstream cell count at which a channel is a river */
} ws_hydro_config;

typedef struct ws_hydro {
    ws_hydro_config config;
    uint32_t cells;
    double cell_area;      /* m^2 */
    double *water_depth;   /* cells^2 metres of standing water */
    double *flow_accum;    /* cells^2 upstream cell count (including self) */
    int8_t *flow_dir;      /* cells^2 D8 index 0..7, or -1 for a sink */
    uint8_t *is_river;     /* cells^2, 1 when a channel */
    uint32_t river_cells;
    double total_water;    /* sum of depth * cell area, m^3 */
} ws_hydro;

ws_hydro_config ws_hydro_config_default(const ws_terrain_config *terrain);

/* Build the flow network and initial water depth for a terrain region. Allocates
 * its arrays; the caller must call ws_hydro_free. */
ws_result ws_hydro_create(ws_hydro *hydro, const ws_hydro_config *config,
                          const ws_terrain *terrain);

void ws_hydro_free(ws_hydro *hydro);

/* Advance the water budget by `dt` seconds: precipitation is added over the
 * whole region, evaporation removes a fraction of the standing water. Returns
 * the signed change in total water (m^3). Total water never goes negative. */
double ws_hydro_budget_step(ws_hydro *hydro, double dt);

/* Water surface elevation at cell centre (terrain height + depth). */
double ws_hydro_surface_height(const ws_hydro *hydro, const ws_terrain *terrain,
                               uint32_t cx, uint32_t cy);

#endif /* WS_WORLD_HYDRO_H */
