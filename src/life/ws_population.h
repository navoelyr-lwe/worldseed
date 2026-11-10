/* Worldseed Engine — aggregate population model for far/mid world levels (P8).
 *
 * The P7 agent ecosystem (ws_ecology) tracks every animal and is the high
 * detail model, but it is too heavy to run for every region of a large world.
 * This module is the coarse companion: it tracks a region's population as
 * continuous numbers plus one plant-biomass scalar, with the same couplings
 * docs/07 asks for (plant production, herbivory, predator-prey transfer,
 * reproduction, death) expressed as coupled growth equations.
 *
 * It is deliberately small and allocation-free so it can be stepped for many
 * regions per tick. It is deterministic with no random draws, so a checkpoint
 * plus the recorded focus path replays bit-identically. It is a planning model
 * (evidence level D2), not a calibrated prediction; the equations and their
 * validity limits are recorded in docs/life/MODEL-KARTI-POPULASYON.md.
 *
 * SI units and seconds. Storage is caller-owned.
 */
#ifndef WS_LIFE_POPULATION_H
#define WS_LIFE_POPULATION_H

#include <stdint.h>

#include "../core/ws_result.h"
#include "ws_species.h"

#define WS_POP_VERSION 1u

typedef struct ws_pop_config {
    uint64_t seed;
    double area_m;                 /* region side length, m (scales capacities) */
    double plant_capacity;         /* total plant biomass a region can hold */
    double plant_growth;           /* logistic growth rate, 1/s */
    double plant_seed;             /* recruitment floor, biomass/s */
    double growth[WS_SPECIES_COUNT];  /* intrinsic per-capita growth rate, 1/s */
    double support[WS_SPECIES_COUNT]; /* biomass/prey units supporting one animal */
    double consume[WS_SPECIES_COUNT]; /* plant biomass eaten per animal per second */
    double plant_half;             /* plant biomass at half-saturation grazing */
    double pred_attack;            /* predator-prey coupling strength, 1/s */
    double pred_half;              /* prey count at half-saturation */
    double pred_gain;              /* predator carrying capacity per prey unit */
    double shock;                  /* capacity multiplier (1.0 = none) */
    double min_viable;             /* below this an isolated population dies out */
    double count_cap[WS_SPECIES_COUNT]; /* hard cap for numerical safety */
} ws_pop_config;

/* Aggregate state of one region. */
typedef struct ws_pop_state {
    double time_s;
    double plant;                    /* total plant biomass */
    double count[WS_SPECIES_COUNT];  /* continuous population per species */
    double births;                   /* cumulative reproduction, for evidence */
    double deaths;                   /* cumulative deaths, for evidence */
    uint64_t extinctions;            /* species lost since seeding */
} ws_pop_state;

ws_pop_config ws_pop_config_default(uint64_t seed);

/* Seed a fresh region state from its deterministic region hash. */
ws_pop_state ws_pop_seed(const ws_pop_config *cfg, uint64_t region_hash);

/* Advance the region by `dt` seconds. Pure and deterministic. */
ws_result ws_pop_step(const ws_pop_config *cfg, ws_pop_state *state, double dt);

/* Total plant capacity for the region given the shock multiplier. */
double ws_pop_plant_capacity(const ws_pop_config *cfg);

/* Sum of all species counts. */
double ws_pop_total(const ws_pop_state *state);

#endif /* WS_LIFE_POPULATION_H */
