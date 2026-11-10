/* Worldseed Engine — ecosystem and population dynamics (P7). */
#include "ws_ecology.h"

#include "../core/ws_math.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

ws_eco_config ws_eco_config_default(uint64_t seed)
{
    ws_eco_config c;
    memset(&c, 0, sizeof(c));
    c.seed = seed;
    c.area_m = 300.0;
    c.resource_cells = 24u;
    c.plant_capacity = 4.0;
    c.plant_growth = 0.002;
    c.plant_seed = 0.0005;
    c.forage_rate = 2.0;
    c.shock_biomass = 1.0;
    c.meta_scale = 2.0;
    c.predation_enabled = 1;
    c.initial_energy = 0.75;
    c.initial_count[WS_SPECIES_DEER] = 6u;
    c.initial_count[WS_SPECIES_FOX] = 3u;
    c.initial_count[WS_SPECIES_RABBIT] = 36u;
    c.initial_count[WS_SPECIES_BOAR] = 6u;
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        c.repro_scale[k] = 1.0;
    }
    c.immigrant_floor = 4u;
    c.immigrant_rate = 0.05; /* one disperser per ~20 s at low numbers */
    c.pred_catch_rate = 1.8;
    c.pred_catch_radius = 4.5;
    c.pred_half_sat = 12.0;
    c.pred_handling_s = 120.0;
    return c;
}

static uint32_t cell_of(const ws_ecology *e, double x, double y, uint32_t *out_iy)
{
    uint32_t n = e->config.resource_cells;
    double c = e->cell_size;
    double ax = x / c;
    double ay = y / c;
    if (ax < 0.0) ax += (double)n;
    if (ay < 0.0) ay += (double)n;
    uint32_t ix = (uint32_t)ax % n;
    uint32_t iy = (uint32_t)ay % n;
    if (out_iy != NULL) {
        *out_iy = iy;
    }
    return ix;
}

ws_result ws_ecology_init(ws_ecology *e, const ws_eco_config *cfg)
{
    if (e == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    ws_eco_config c = (cfg != NULL) ? *cfg : ws_eco_config_default(0u);
    if (c.resource_cells == 0u || c.resource_cells > WS_ECOLOGY_MAX_CELLS ||
        !(c.area_m > 0.0) || !(c.plant_capacity > 0.0)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    memset(e, 0, sizeof(*e));
    e->config = c;
    e->capacity = WS_ECOLOGY_MAX_AGENTS;
    e->cell_size = c.area_m / (double)c.resource_cells;

    size_t cells = (size_t)c.resource_cells * c.resource_cells;
    e->plants = (double *)malloc(cells * sizeof(double));
    if (e->plants == NULL) {
        return WS_ERR_OUT_OF_MEMORY;
    }
    /* Start at half capacity: a spread-out resource the founders can grow into. */
    for (size_t i = 0; i < cells; ++i) {
        e->plants[i] = c.plant_capacity * 0.5;
    }

    ws_rng_init(&e->rng, c.seed, 0x7007u);
    e->sample_interval_steps = 50u;

    /* Seed the founder population, species by species, in a fixed order. */
    uint32_t idx = 0u;
    for (int s = 0; s < WS_SPECIES_COUNT; ++s) {
        for (uint32_t i = 0; i < c.initial_count[s] && idx < e->capacity; ++i) {
            ws_eco_agent *a = &e->agents[idx];
            memset(a, 0, sizeof(*a));
            a->alive = 1;
            a->species = s;
            a->sex = (int)(ws_rng_next_u64(&e->rng) & 1u);
            a->x = ws_rng_next_double(&e->rng) * c.area_m;
            a->y = ws_rng_next_double(&e->rng) * c.area_m;
            a->heading = ws_rng_next_double(&e->rng) * 6.283185307179586;
            a->energy = c.initial_energy;
            /* Start founders at maturity so the ecosystem turns over promptly. */
            a->age_s = ws_species_get((ws_species_kind)s)->eco_maturity_s;
            ++idx;
        }
    }
    e->alive_count = idx;
    return WS_OK;
}

void ws_ecology_free(ws_ecology *e)
{
    if (e == NULL) {
        return;
    }
    free(e->plants);
    e->plants = NULL;
}

uint32_t ws_ecology_population(const ws_ecology *e, int species)
{
    if (e == NULL || species < 0 || species >= WS_SPECIES_COUNT) {
        return 0u;
    }
    uint32_t n = 0u;
    for (uint32_t i = 0; i < e->alive_count; ++i) {
        if (e->agents[i].alive && e->agents[i].species == species) {
            ++n;
        }
    }
    return n;
}

double ws_ecology_plant_biomass(const ws_ecology *e)
{
    if (e == NULL || e->plants == NULL) {
        return 0.0;
    }
    uint32_t n = e->config.resource_cells;
    double total = 0.0;
    for (size_t i = 0; i < (size_t)n * n; ++i) {
        total += e->plants[i];
    }
    return total;
}

double ws_ecology_plant_capacity_total(const ws_ecology *e)
{
    if (e == NULL) {
        return 0.0;
    }
    double n = (double)(e->config.resource_cells * e->config.resource_cells);
    return n * e->config.plant_capacity * e->config.shock_biomass;
}

/* Logistic growth of every plant cell toward the (shocked) capacity, plus a
 * small recruitment (seed-rain) floor so a cell grazed to zero is not an
 * absorbing state. */
static void grow_plants(ws_ecology *e, double dt)
{
    uint32_t n = e->config.resource_cells;
    double cap = e->config.plant_capacity * e->config.shock_biomass;
    double g = e->config.plant_growth;
    double seed = e->config.plant_seed * dt;
    size_t cells = (size_t)n * n;
    for (size_t i = 0; i < cells; ++i) {
        double b = e->plants[i];
        double bnext = b + g * b * (1.0 - b / cap) * dt + seed * (1.0 - b / cap);
        if (bnext < 0.0) {
            bnext = 0.0;
        }
        if (bnext > cap) {
            bnext = cap;
        }
        e->plants[i] = bnext;
    }
}

/* Graze from the plant cell nearest the agent; returns biomass actually eaten. */
static double graze(ws_ecology *e, ws_eco_agent *a, double want)
{
    uint32_t iy;
    uint32_t ix = cell_of(e, a->x, a->y, &iy);
    size_t idx = (size_t)iy * e->config.resource_cells + ix;
    double have = e->plants[idx];
    double taken = (want < have) ? want : have;
    e->plants[idx] = have - taken;
    return taken;
}

static ws_eco_agent *spawn_agent(ws_ecology *e, int species, double x, double y,
                                 double energy, double age_s)
{
    if (e->alive_count >= e->capacity) {
        return NULL;
    }
    ws_eco_agent *a = &e->agents[e->alive_count];
    memset(a, 0, sizeof(*a));
    a->alive = 1;
    a->species = species;
    a->sex = (int)(ws_rng_next_u64(&e->rng) & 1u);
    a->x = x;
    a->y = y;
    a->heading = ws_rng_next_double(&e->rng) * 6.283185307179586;
    a->energy = ws_clamp_double(energy, 0.0, 1.0);
    a->age_s = age_s;
    e->alive_count += 1u;
    return a;
}

static int spawn_offspring(ws_ecology *e, ws_eco_agent *parent, int species)
{
    const ws_species_info *si = ws_species_get((ws_species_kind)species);
    double x = parent->x + (ws_rng_next_double(&e->rng) - 0.5) * 5.0;
    double y = parent->y + (ws_rng_next_double(&e->rng) - 0.5) * 5.0;
    ws_eco_agent *child = spawn_agent(e, species, x, y, si->juvenile_frac, 0.0);
    if (child == NULL) {
        return 0;
    }
    e->births += 1u;
    parent->offspring += 1u;
    return 1;
}

static void step_agent(ws_ecology *e, uint32_t i, double dt)
{
    ws_eco_agent *a = &e->agents[i];
    const ws_species_info *si = ws_species_get((ws_species_kind)a->species);

    a->age_s += dt;
    if (a->age_s >= si->eco_lifespan_s) {
        a->alive = 0;
        e->deaths += 1u;
        e->age_deaths += 1u;
        e->deaths_by_species[a->species] += 1u;
        return;
    }

    /* Locomotion. A predator that is below its satiation level steers toward the
     * nearest prey within its sense range; a herbivore that senses a predator
     * flees from it at top speed; everyone else takes a small deterministic
     * wander. Speed is the species' walk speed unless hunting or fleeing. */
    double speed = ws_species_walk_speed((ws_species_kind)a->species);
    if (a->hunt_cooldown > 0.0) {
        a->hunt_cooldown -= dt;
    }
    int fleeing = 0;
    if (si->diet != WS_DIET_CARNIVORE && e->config.predation_enabled) {
        double range = si->sense_range;
        double best_d2 = range * range;
        double bx = 0.0, by = 0.0;
        for (uint32_t k = 0; k < e->alive_count; ++k) {
            const ws_eco_agent *q = &e->agents[k];
            if (!q->alive || q->species == a->species) {
                continue;
            }
            const ws_species_info *qi = ws_species_get((ws_species_kind)q->species);
            if (qi->diet != WS_DIET_CARNIVORE) {
                continue;
            }
            double dx = q->x - a->x;
            double dy = q->y - a->y;
            double d2 = dx * dx + dy * dy;
            if (d2 <= best_d2) {
                best_d2 = d2;
                bx = dx;
                by = dy;
                fleeing = 1;
            }
        }
        if (fleeing) {
            a->heading = atan2(-by, -bx); /* away from the predator */
            speed = si->max_speed;
        }
    }
    int hunting = 0;
    if (!fleeing && si->diet == WS_DIET_CARNIVORE && e->config.predation_enabled &&
        a->hunt_cooldown <= 0.0 && a->energy < 0.85) {
        int prey_kind = ws_species_prey((ws_species_kind)a->species);
        double range = si->sense_range;
        double best_d2 = range * range;
        double bx = 0.0, by = 0.0;
        for (uint32_t k = 0; k < e->alive_count; ++k) {
            const ws_eco_agent *q = &e->agents[k];
            if (!q->alive || q->species != prey_kind) {
                continue;
            }
            double dx = q->x - a->x;
            double dy = q->y - a->y;
            double d2 = dx * dx + dy * dy;
            if (d2 <= best_d2) {
                best_d2 = d2;
                bx = dx;
                by = dy;
                hunting = 1;
            }
        }
        if (hunting) {
            a->heading = atan2(by, bx);
            speed = si->max_speed;
        }
    }
    if (!hunting && !fleeing) {
        a->heading += (ws_rng_next_double(&e->rng) - 0.5) * 0.8;
    }
    double step = speed * dt;
    a->x += cos(a->heading) * step;
    a->y += sin(a->heading) * step;
    double area = e->config.area_m;
    if (a->x < 0.0) a->x += area;
    if (a->x >= area) a->x -= area;
    if (a->y < 0.0) a->y += area;
    if (a->y >= area) a->y -= area;
    a->dist_travelled += step;

    /* Grazing: demand-driven. An animal eats only what it needs to refill its
     * reserve (up to a bite at a time, at a fixed attempt rate), so per-capita
     * consumption is bounded by metabolism and the plant resource sets a
     * carrying capacity rather than being mined without limit. Herbivores fill
     * their reserve; a generalist carnivore uses plant food only as a low-quality
     * survival buffer and still prefers to hunt above that level. */
    {
        double target = 1.0;
        double gain = si->forage_gain;
        if (si->diet == WS_DIET_CARNIVORE) {
            target = 0.35;
            gain = si->plant_gain;
        }
        double deficit = target - a->energy;
        if (gain > 0.0 && deficit > 0.01) {
            double need = deficit / gain; /* biomass to refill to the target */
            double attempts = e->config.forage_rate * dt;
            int whole = (int)attempts;
            double frac = attempts - (double)whole;
            if (ws_rng_next_double(&e->rng) < frac) {
                whole += 1;
            }
            for (int b = 0; b < whole && need > 1e-9; ++b) {
                double want = (si->bite < need) ? si->bite : need;
                double taken = graze(e, a, want);
                if (taken <= 0.0) {
                    break;
                }
                a->energy = ws_clamp_double(a->energy + taken * gain, 0.0, 1.0);
                a->bites += 1u;
                e->forage_events += 1u;
                need -= taken;
            }
        }
    }

    /* Metabolism: basal plus movement, scaled by the ecosystem's metabolic
     * timescale (the config field lets the carrying capacity be tuned against
     * the plant resource without editing the species table). */
    double cost = (si->basal_rate + si->move_rate * speed) * e->config.meta_scale * dt;
    a->energy = ws_clamp_double(a->energy - cost, 0.0, 1.0);

    /* Predation: a hunt that has closed on prey may capture it, after which the
     * predator is satiated for a handling period. Capture success falls as prey
     * become rare (a Holling type III response: prey get a refuge at low
     * density), which is what lets the predator and prey persist together
     * instead of the predator over-hunting the prey to extinction. */
    if (si->diet == WS_DIET_CARNIVORE && e->config.predation_enabled &&
        hunting) {
        int prey_kind = ws_species_prey((ws_species_kind)a->species);
        uint32_t prey_count = 0;
        for (uint32_t k = 0; k < e->alive_count; ++k) {
            if (e->agents[k].alive && e->agents[k].species == prey_kind) {
                prey_count += 1u;
            }
        }
        /* Half-saturation: below the configured prey count, capture is
         * suppressed (Holling type III refuge). */
        double density = (double)prey_count /
                         ((double)prey_count + e->config.pred_half_sat);
        double catch_r = e->config.pred_catch_radius;
        for (uint32_t k = 0; k < e->alive_count; ++k) {
            ws_eco_agent *prey = &e->agents[k];
            if (!prey->alive || prey->species != prey_kind) {
                continue;
            }
            double dx = prey->x - a->x;
            double dy = prey->y - a->y;
            /* Compare on the torus so a chase across the seam still closes. */
            if (dx > area * 0.5) dx -= area;
            if (dx < -area * 0.5) dx += area;
            if (dy > area * 0.5) dy -= area;
            if (dy < -area * 0.5) dy += area;
            if (dx * dx + dy * dy <= catch_r * catch_r) {
                double catch_prob = e->config.pred_catch_rate * density;
                if (ws_rng_next_double(&e->rng) < catch_prob * dt) {
                    double gained = 0.75 * prey->energy + 0.35;
                    a->energy = ws_clamp_double(a->energy + gained, 0.0, 1.0);
                    prey->alive = 0;
                    a->kills += 1u;
                    a->hunt_cooldown = e->config.pred_handling_s;
                    e->predation_events += 1u;
                }
                break;
            }
        }
    }

    /* Reproduction: a mature animal breeds at a rate that rises with its energy
     * surplus above the breeding threshold. A graded response (rather than a
     * hard switch) damps the boom-bust cycles that otherwise drive a small
     * predator-prey system to extinction: a population near its resource limit
     * is energy-stressed and its birth rate falls off smoothly. */
    if (a->age_s >= si->eco_maturity_s && a->energy > si->repro_energy) {
        double surplus = (a->energy - si->repro_energy) /
                         (1.0 - si->repro_energy);
        double rate = si->repro_rate * e->config.repro_scale[a->species] * surplus;
        if (ws_rng_next_double(&e->rng) < rate * dt) {
            if (spawn_offspring(e, a, a->species)) {
                a->energy = ws_clamp_double(a->energy - si->repro_cost, 0.0, 1.0);
            }
        }
    }

    if (a->energy <= 0.0) {
        a->alive = 0;
        e->deaths += 1u;
        e->starvation_deaths += 1u;
        e->deaths_by_species[a->species] += 1u;
        return;
    }

}

/* Compact the agent array so dead agents are removed and alive ones stay in a
 * stable order; determinism is preserved because the order is index order. */
static void compact(ws_ecology *e)
{
    uint32_t w = 0u;
    for (uint32_t i = 0; i < e->alive_count; ++i) {
        if (e->agents[i].alive) {
            if (w != i) {
                e->agents[w] = e->agents[i];
            }
            ++w;
        }
    }
    e->alive_count = w;
}

static void sample(ws_ecology *e)
{
    uint32_t head = e->sample_head;
    for (int s = 0; s < WS_SPECIES_COUNT; ++s) {
        float pop = (float)ws_ecology_population(e, s);
        e->pop_series[head][s] = pop;
    }
    e->sample_head = (head + 1u) % WS_ECOLOGY_MAX_SAMPLES;
    if (e->sample_count < WS_ECOLOGY_MAX_SAMPLES) {
        e->sample_count += 1u;
    }
}

/* Dispersal from the surrounding world. The modelled region is one patch of the
 * larger 2x2 km world, so a species that has fallen to very low numbers can
 * receive immigrants from outside. This keeps a small population from being
 * trapped in an absorbing zero state without changing the local dynamics at
 * healthy abundance. */
static void disperse(ws_ecology *e, double dt)
{
    if (e->config.immigrant_floor == 0u || !(e->config.immigrant_rate > 0.0)) {
        return;
    }
    for (int s = 0; s < WS_SPECIES_COUNT; ++s) {
        if (ws_ecology_population(e, s) >= e->config.immigrant_floor) {
            continue;
        }
        if (ws_rng_next_double(&e->rng) < e->config.immigrant_rate * dt) {
            const ws_species_info *si = ws_species_get((ws_species_kind)s);
            double x = ws_rng_next_double(&e->rng) * e->config.area_m;
            double y = ws_rng_next_double(&e->rng) * e->config.area_m;
            spawn_agent(e, s, x, y, e->config.initial_energy, si->eco_maturity_s);
        }
    }
}

ws_result ws_ecology_step(ws_ecology *e, double dt)
{
    if (e == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (!(dt > 0.0) || !isfinite(dt)) {
        return WS_ERR_INVALID_ARGUMENT;
    }
    grow_plants(e, dt);
    uint32_t n = e->alive_count; /* newborn agents wait until the next step */
    for (uint32_t i = 0; i < n; ++i) {
        if (e->agents[i].alive) {
            step_agent(e, i, dt);
        }
    }
    compact(e);
    disperse(e, dt);
    e->time_s += dt;

    uint64_t step_no = (uint64_t)(e->time_s / dt + 0.5);
    if (e->sample_interval_steps > 0u && step_no % e->sample_interval_steps == 0u) {
        sample(e);
    }
    return WS_OK;
}
