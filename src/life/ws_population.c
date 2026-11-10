/* Worldseed Engine — aggregate population model for far/mid world levels (P8). */
#include "ws_population.h"

#include <math.h>

#include "../core/ws_math.h"

#define WS_POP_TINY 1e-9

ws_pop_config ws_pop_config_default(uint64_t seed)
{
    ws_pop_config c;
    c.seed = seed;
    c.area_m = 512.0;
    c.plant_capacity = 100.0;
    c.plant_growth = 0.002;
    c.plant_seed = 0.0005;
    c.plant_half = 20.0;

    /* growth: intrinsic per-capita rate; support: plant biomass per animal;
     * consume: plant biomass eaten per animal per second. */
    c.growth[WS_SPECIES_DEER] = 0.0006;
    c.support[WS_SPECIES_DEER] = 6.0;
    c.consume[WS_SPECIES_DEER] = 0.0010;

    c.growth[WS_SPECIES_FOX] = 0.0005;
    c.support[WS_SPECIES_FOX] = 0.0;   /* carnivore: no plant share */
    c.consume[WS_SPECIES_FOX] = 0.0;

    c.growth[WS_SPECIES_RABBIT] = 0.0014;
    c.support[WS_SPECIES_RABBIT] = 1.4;
    c.consume[WS_SPECIES_RABBIT] = 0.00022;

    c.growth[WS_SPECIES_BOAR] = 0.0005;
    c.support[WS_SPECIES_BOAR] = 5.0;
    c.consume[WS_SPECIES_BOAR] = 0.0013;

    c.pred_attack = 0.0016;
    c.pred_half = 20.0;
    c.pred_gain = 0.14; /* fox capacity = pred_gain * rabbit count */

    c.shock = 1.0;
    c.min_viable = 0.5;
    c.count_cap[WS_SPECIES_DEER] = 60.0;
    c.count_cap[WS_SPECIES_FOX] = 40.0;
    c.count_cap[WS_SPECIES_RABBIT] = 400.0;
    c.count_cap[WS_SPECIES_BOAR] = 60.0;
    return c;
}

double ws_pop_plant_capacity(const ws_pop_config *cfg)
{
    if (cfg == NULL) {
        return 0.0;
    }
    return cfg->plant_capacity * cfg->shock;
}

/* Deterministic pseudo-random draw in [0,1) from a hash and a channel, so a
 * fresh region starts from a varied but reproducible state without needing an
 * RNG stream that would have to be checkpointed. */
static double hash_unit(uint64_t h, uint32_t channel)
{
    uint64_t x = h ^ ((uint64_t)channel * 0x9e3779b97f4a7c15ULL);
    x ^= x >> 30; x *= 0xbf58476d1ce4e5b9ULL;
    x ^= x >> 27; x *= 0x94d049bb133111ebULL;
    x ^= x >> 31;
    return (double)(x >> 11) / 9007199254740992.0; /* 53 bits -> [0,1) */
}

ws_pop_state ws_pop_seed(const ws_pop_config *cfg, uint64_t region_hash)
{
    ws_pop_state s;
    s.time_s = 0.0;
    s.plant = 0.0;
    s.births = 0.0;
    s.deaths = 0.0;
    s.extinctions = 0u;
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        s.count[k] = 0.0;
    }
    if (cfg == NULL) {
        return s;
    }
    double cap = ws_pop_plant_capacity(cfg);
    /* Start at half capacity with a per-region variation in [0.4, 0.6]. */
    s.plant = cap * (0.4 + 0.2 * hash_unit(region_hash, 0u));
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        /* Founders: a fraction of the hard cap, varied per region/species. */
        double frac = 0.15 + 0.15 * hash_unit(region_hash, (uint32_t)(k + 1u));
        s.count[k] = cfg->count_cap[k] * frac;
    }
    return s;
}

ws_result ws_pop_step(const ws_pop_config *cfg, ws_pop_state *state, double dt)
{
    if (cfg == NULL || state == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0) || !isfinite(dt)) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    double cap = ws_pop_plant_capacity(cfg);
    double p = state->plant;
    if (p < 0.0) { p = 0.0; }
    if (p > cap) { p = cap; }

    /* Grazing pressure (herbivores + omnivores). */
    double grazing = 0.0;
    double sat = p / (p + cfg->plant_half);
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        if (cfg->consume[k] > 0.0) {
            grazing += cfg->consume[k] * state->count[k] * sat;
        }
    }
    double plant_growth = cfg->plant_growth * p * (1.0 - p / cap) +
                          cfg->plant_seed * (1.0 - p / cap);
    double dP = plant_growth - grazing;
    p += dP * dt;
    if (p < 0.0) { p = 0.0; }
    if (p > cap) { p = cap; }
    state->plant = p;

    /* Predation: the fox consumes rabbits (the fixed food-web edge). */
    double rabbits = state->count[WS_SPECIES_RABBIT];
    double hunt = cfg->pred_attack * state->count[WS_SPECIES_FOX] *
                  (rabbits / (rabbits + cfg->pred_half));
    double rabbit_extra_loss = hunt;

    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        double n = state->count[k];
        if (n < 0.0) { n = 0.0; }

        double ke;
        if (k == WS_SPECIES_FOX) {
            ke = cfg->pred_gain * rabbits; /* predator capacity from prey */
        } else {
            ke = (cfg->support[k] > 0.0) ? (p / cfg->support[k]) : 0.0;
        }

        double dN;
        if (ke > WS_POP_TINY) {
            dN = cfg->growth[k] * n * (1.0 - n / ke);
        } else {
            dN = -cfg->growth[k] * n; /* no resource: the population starves */
        }
        double next = n + dN * dt;
        if (k == WS_SPECIES_RABBIT) {
            next -= rabbit_extra_loss * dt;
        }
        if (next < 0.0) { next = 0.0; }
        if (next > cfg->count_cap[k]) { next = cfg->count_cap[k]; }

        if (next > n) {
            state->births += (next - n);
        } else {
            state->deaths += (n - next);
        }
        if (next > 0.0 && next < cfg->min_viable) {
            next = 0.0;
            state->extinctions += 1u;
        }
        state->count[k] = next;
    }

    state->time_s += dt;
    return WS_OK;
}

double ws_pop_total(const ws_pop_state *state)
{
    if (state == NULL) {
        return 0.0;
    }
    double total = 0.0;
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        total += state->count[k];
    }
    return total;
}
