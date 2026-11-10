/* Worldseed Engine — ecosystem and population dynamics (P7).
 *
 * A deterministic, headless, agent-based ecosystem over a coarse plant-resource
 * grid. It models the couplings docs/07 asks for: plant production, herbivory,
 * predator-prey energy transfer, reproduction, death and population. Each
 * animal is an agent carrying its species' diet and metabolic rates
 * (ws_species.h); herbivores and omnivores graze the plant grid, carnivores hunt
 * their prey species, all of them spend reserves by basal and movement
 * metabolism, reproduce when mature and well-fed, and die of starvation or old
 * age.
 *
 * Design rules (enforced by the P7 tests):
 *  - Conservation: an animal's reserve stays in [0,1]; eating never creates more
 *    energy than the biomass taken times the species' gain; a bite cannot remove
 *    more plant biomass than the cell holds.
 *  - Boundedness: plant growth is logistic with a per-cell capacity, so resources
 *    cannot grow without limit; the population is capped by the agent array.
 *  - Determinism: one RNG stream per ecology, fixed agent iteration order and
 *    index-broken ties, so the same seed gives a bit-identical run.
 *
 * SI units and seconds. This is an ecological planning model, not a calibrated
 * prediction of a specific reserve (documented in docs/life/MODEL-KARTI-EKOLOJI.md).
 * Storage is caller-owned; the only allocation is the plant grid.
 */
#ifndef WS_LIFE_ECOLOGY_H
#define WS_LIFE_ECOLOGY_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "../core/ws_rng.h"
#include "../world/ws_event.h"
#include "ws_species.h"

#define WS_ECOLOGY_VERSION 1u
#define WS_ECOLOGY_MAX_AGENTS 512u
#define WS_ECOLOGY_MAX_CELLS 64u
#define WS_ECOLOGY_MAX_SAMPLES 256u
#define WS_ECOLOGY_MAX_EVENTS 1024u

typedef struct ws_eco_config {
    uint64_t seed;
    double area_m;           /* region side length, m (resource grid covers it) */
    uint32_t resource_cells; /* grid cells per side (<= WS_ECOLOGY_MAX_CELLS) */
    double plant_capacity;   /* max biomass per cell, arbitrary units */
    double plant_growth;     /* logistic growth rate, 1/s */
    double plant_seed;       /* recruitment floor, biomass/s per cell, so a cell
                              * grazed to zero can recover (seed rain) */
    double forage_rate;      /* grazing/capture attempts per second */
    double shock_biomass;    /* resource-shock multiplier on capacity (1.0 = none) */
    double meta_scale;       /* multiplies species metabolic rates; sets the
                              * starvation timescale and hence the carrying
                              * capacity of the plant resource */
    int predation_enabled;
    double initial_energy;   /* starting reserve fraction of the founder animals */
    uint32_t initial_count[WS_SPECIES_COUNT];
    double repro_scale[WS_SPECIES_COUNT]; /* multiplies each species' repro rate */
    uint32_t immigrant_floor; /* a species below this many individuals may receive
                               * dispersers from the surrounding world (the
                               * modelled patch is one part of the 2x2 km world) */
    double immigrant_rate;    /* disperser arrivals per second at low population */
    double pred_catch_rate;   /* max capture probability per second at high prey */
    double pred_catch_radius; /* m, distance within which a strike can connect */
    double pred_half_sat;     /* prey count at which capture is half suppressed */
    double pred_handling_s;   /* s a predator rests after a kill (handling time) */
} ws_eco_config;

ws_eco_config ws_eco_config_default(uint64_t seed);

typedef struct ws_eco_agent {
    int alive;
    int species;        /* ws_species_kind */
    int sex;            /* 0 female, 1 male */
    double x, y;        /* m */
    double heading;     /* rad */
    double energy;      /* 0..1 */
    double age_s;
    double dist_travelled; /* m */
    double hunt_cooldown;  /* s of satiation left before hunting again */
    uint64_t bites;     /* forage events */
    uint64_t kills;     /* successful predation events */
    uint32_t offspring;
} ws_eco_agent;

typedef struct ws_ecology {
    ws_eco_config config;
    ws_rng rng;

    ws_eco_agent agents[WS_ECOLOGY_MAX_AGENTS];
    uint32_t capacity;   /* WS_ECOLOGY_MAX_AGENTS */
    uint32_t alive_count;

    double *plants;      /* resource_cells^2 biomass, caller-viewed via accessor */
    double cell_size;    /* m per grid cell */

    double time_s;
    uint64_t births;
    uint64_t deaths;                 /* total */
    uint64_t deaths_by_species[WS_SPECIES_COUNT];
    uint64_t predation_events;
    uint64_t forage_events;
    uint64_t starvation_deaths;
    uint64_t age_deaths;

    /* Down-sampled population time series (one sample per interval). */
    uint32_t sample_interval_steps;
    uint32_t sample_head;
    uint32_t sample_count;
    float pop_series[WS_ECOLOGY_MAX_SAMPLES][WS_SPECIES_COUNT];

    /* Life event log: discrete, observable things the simulation actually
     * produced (ate, drank, born, died, killed). The cinematic director reads
     * this and never invents an event (docs/09). A fixed ring keeps the most
     * recent events; the director also gets a running total. */
    ws_event events[WS_ECOLOGY_MAX_EVENTS];
    uint32_t event_count;
    uint64_t next_event_sequence;
    uint64_t events_dropped;
    uint64_t events_total;
} ws_ecology;

/* Initialise and seed the founder population. Allocates the plant grid; the
 * caller must call ws_ecology_free. */
ws_result ws_ecology_init(ws_ecology *e, const ws_eco_config *cfg);

void ws_ecology_free(ws_ecology *e);

/* Advance the whole ecosystem by one step of `dt` seconds. */
ws_result ws_ecology_step(ws_ecology *e, double dt);

/* Alive population of a species. */
uint32_t ws_ecology_population(const ws_ecology *e, int species);

/* Total plant biomass over the grid. */
double ws_ecology_plant_biomass(const ws_ecology *e);

/* Peak plant capacity given the shock multiplier (for budget checks). */
double ws_ecology_plant_capacity_total(const ws_ecology *e);

/* --- life event log (P9) -------------------------------------------------- */

/* Number of life events retained in the ring (oldest first via _at). */
uint32_t ws_ecology_event_count(const ws_ecology *e);

/* Read a retained life event by age: index 0 is the oldest retained. */
ws_result ws_ecology_event_at(const ws_ecology *e, uint32_t index, ws_event *out);

/* Total life events produced since init (including ones aged out of the ring). */
uint64_t ws_ecology_events_total(const ws_ecology *e);

/* Number of events overwritten since init. */
uint64_t ws_ecology_events_dropped(const ws_ecology *e);

#endif /* WS_LIFE_ECOLOGY_H */
