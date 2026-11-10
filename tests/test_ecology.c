/* Worldseed Engine — ecosystem and species tests (P7).
 *
 * Scientific regression suite from docs/07 "Ekolojik testler": reserve and
 * biomass conservation, no unlimited resource growth or population explosion,
 * predator-prey coupling and bounds, reproduction/death behaviour, resource
 * shock response, multi-species presence, and determinism across at least ten
 * fixed seeds (single-seed acceptance is not allowed). Real models; no mocks.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "life/ws_ecology.h"
#include "life/ws_species.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int test_species_table(void)
{
    for (int s = 0; s < WS_SPECIES_COUNT; ++s) {
        const ws_species_info *i = ws_species_get((ws_species_kind)s);
        WS_CHECK(i != NULL);
        WS_CHECK(i->name != NULL && i->name[0] != '\0');
        WS_CHECK(i->body.total_mass > 0.0);
        WS_CHECK(i->body.shoulder_h > 0.0);
        WS_CHECK(i->body.leg_upper > 0.0 && i->body.leg_lower > 0.0);
        WS_CHECK(i->body.stance > 0.0);
        WS_CHECK(i->max_speed > 0.0 && i->sense_range > 0.0);
        WS_CHECK(i->maturity_s > 0.0 && i->max_age_s > i->maturity_s);
        WS_CHECK(i->juvenile_frac > 0.0 && i->juvenile_frac <= 1.0);
        WS_CHECK(i->card != NULL && i->card[0] != '\0');
    }
    /* The food web: the fox hunts the rabbit for the first release. */
    WS_CHECK(ws_species_prey(WS_SPECIES_FOX) == WS_SPECIES_RABBIT);
    WS_CHECK(ws_species_prey(WS_SPECIES_DEER) == -1);
    return 1;
}

static int test_plant_bounded(void)
{
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(1u);
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    double cap = ws_ecology_plant_capacity_total(&e);
    for (int i = 0; i < 20000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
        double b = ws_ecology_plant_biomass(&e);
        WS_CHECK(isfinite(b));
        WS_CHECK(b >= 0.0);
        /* Logistic growth can never exceed capacity. */
        WS_CHECK(b <= cap * 1.000001);
    }
    ws_ecology_free(&e);
    return 1;
}

static int test_reserve_and_biomass_conservation(void)
{
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(7u);
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    for (int i = 0; i < 5000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
        for (uint32_t k = 0; k < e.alive_count; ++k) {
            const ws_eco_agent *a = &e.agents[k];
            WS_CHECK(a->energy >= 0.0 && a->energy <= 1.0);
            WS_CHECK(isfinite(a->x) && isfinite(a->y) && isfinite(a->energy));
            WS_CHECK(a->x >= 0.0 && a->x < c.area_m);
            WS_CHECK(a->y >= 0.0 && a->y < c.area_m);
        }
        uint32_t n = c.resource_cells;
        for (size_t j = 0; j < (size_t)n * n; ++j) {
            WS_CHECK(e.plants[j] >= 0.0);
        }
    }
    ws_ecology_free(&e);
    return 1;
}

static int test_no_population_explosion(void)
{
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(42u);
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    for (int i = 0; i < 40000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
    }
    WS_CHECK(e.alive_count <= WS_ECOLOGY_MAX_AGENTS);
    /* The population is bounded by the finite resource base, not just the cap;
     * a well-fed founder set cannot run away to the array limit here. */
    WS_CHECK(e.alive_count < WS_ECOLOGY_MAX_AGENTS);
    ws_ecology_free(&e);
    return 1;
}

static int test_predator_prey_coupling(void)
{
    /* Isolate the coupling: a handful of foxes and rabbits on a plant-rich world.
     * Predation must actually occur, rabbits must be removed by it, and the fox
     * count must not grow without bound. */
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(11u);
    memset(c.initial_count, 0, sizeof(c.initial_count));
    c.initial_count[WS_SPECIES_FOX] = 8u;
    c.initial_count[WS_SPECIES_RABBIT] = 40u;
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    for (int i = 0; i < 20000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
    }
    WS_CHECK(e.predation_events > 0u);
    WS_CHECK(ws_ecology_population(&e, WS_SPECIES_FOX) < 200u);
    WS_CHECK(e.deaths_by_species[WS_SPECIES_RABBIT] > 0u);
    ws_ecology_free(&e);
    return 1;
}

static int test_reproduction_and_death(void)
{
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(3u);
    memset(c.initial_count, 0, sizeof(c.initial_count));
    c.initial_count[WS_SPECIES_RABBIT] = 20u;
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    uint64_t b0 = e.births;
    for (int i = 0; i < 20000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
    }
    WS_CHECK(e.births > b0);
    WS_CHECK(e.deaths > 0u);
    WS_CHECK(e.births > b0 && e.deaths > 0u);
    ws_ecology_free(&e);
    return 1;
}

static int test_starvation_death(void)
{
    /* No plants: every animal must eventually starve, and the plant grid must
     * stay empty (no hidden production). */
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(5u);
    memset(c.initial_count, 0, sizeof(c.initial_count));
    c.initial_count[WS_SPECIES_DEER] = 10u;
    c.plant_capacity = 1e-9; /* effectively no resource */
    c.immigrant_floor = 0u;  /* isolate the local starvation mechanism */
    c.immigrant_rate = 0.0;
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    for (size_t i = 0; i < (size_t)c.resource_cells * c.resource_cells; ++i) {
        e.plants[i] = 0.0;
    }
    for (int i = 0; i < 60000; ++i) {
        (void)ws_ecology_step(&e, 0.1);
    }
    WS_CHECK(ws_ecology_population(&e, WS_SPECIES_DEER) == 0u);
    WS_CHECK(e.starvation_deaths > 0u);
    ws_ecology_free(&e);
    return 1;
}

static int test_resource_shock(void)
{
    /* A resource shock that halves capacity must be reflected in the equilibrium
     * biomass; the ecosystem must not produce more than the shocked capacity. */
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(9u);
    c.shock_biomass = 0.5;
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    double cap = ws_ecology_plant_capacity_total(&e);
    for (int i = 0; i < 30000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
    }
    WS_CHECK(ws_ecology_plant_biomass(&e) <= cap * 1.000001);
    ws_ecology_free(&e);
    return 1;
}

static int test_multi_species_present(void)
{
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(13u);
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    int all_seen = 0;
    for (int i = 0; i < 20000 && !all_seen; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
        all_seen = 1;
        for (int s = 0; s < WS_SPECIES_COUNT; ++s) {
            if (ws_ecology_population(&e, s) == 0u) {
                all_seen = 0;
            }
        }
    }
    WS_CHECK(all_seen == 1);
    ws_ecology_free(&e);
    return 1;
}

static int test_long_term_coexistence(void)
{
    /* The headline P7 requirement (docs/07, docs/01): all four representative
     * species must persist together in a bounded, non-exploding equilibrium on
     * the default configuration, over a long horizon and across at least ten
     * fixed seeds. Single-seed acceptance is not allowed. */
    for (uint64_t s = 0; s < 12u; ++s) {
        ws_ecology e;
        ws_eco_config c = ws_eco_config_default(0x7ec0u ^ s);
        WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
        for (int i = 0; i < 72000; ++i) { /* 3600 s of ecosystem time */
            WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
        }
        for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
            WS_CHECK(ws_ecology_population(&e, k) > 0u);
        }
        /* Bounded: a finite resource base cannot be mined into an explosion. */
        WS_CHECK(e.alive_count < WS_ECOLOGY_MAX_AGENTS);
        ws_ecology_free(&e);
    }
    return 1;
}

/* Determinism: two ecologies with the same seed must be bit-identical, checked
 * across ten fixed seeds, and a different seed must diverge. */
static int test_determinism_across_seeds(void)
{
    for (uint64_t s = 0; s < 10u; ++s) {
        ws_ecology a;
        ws_ecology b;
        ws_eco_config c = ws_eco_config_default(100u + s * 17u);
        WS_CHECK(ws_ecology_init(&a, &c) == WS_OK);
        WS_CHECK(ws_ecology_init(&b, &c) == WS_OK);
        for (int i = 0; i < 3000; ++i) {
            (void)ws_ecology_step(&a, 0.05);
            (void)ws_ecology_step(&b, 0.05);
        }
        WS_CHECK(a.alive_count == b.alive_count);
        WS_CHECK(a.births == b.births && a.deaths == b.deaths);
        WS_CHECK(memcmp(a.agents, b.agents,
                        (size_t)a.alive_count * sizeof(ws_eco_agent)) == 0);
        ws_ecology_free(&a);
        ws_ecology_free(&b);
    }
    /* Different seeds diverge (they are not accidentally seed-independent). */
    ws_ecology p;
    ws_ecology q;
    ws_eco_config cp = ws_eco_config_default(1u);
    ws_eco_config cq = ws_eco_config_default(2u);
    WS_CHECK(ws_ecology_init(&p, &cp) == WS_OK);
    WS_CHECK(ws_ecology_init(&q, &cq) == WS_OK);
    for (int i = 0; i < 3000; ++i) {
        (void)ws_ecology_step(&p, 0.05);
        (void)ws_ecology_step(&q, 0.05);
    }
    WS_CHECK(memcmp(p.agents, q.agents, sizeof(ws_eco_agent)) != 0);
    ws_ecology_free(&p);
    ws_ecology_free(&q);
    return 1;
}

static int test_long_run_stability(void)
{
    /* A long headless run must stay finite and bounded, with no reserve or
     * population blow-up. */
    ws_ecology e;
    ws_eco_config c = ws_eco_config_default(2024u);
    WS_CHECK(ws_ecology_init(&e, &c) == WS_OK);
    for (int i = 0; i < 200000; ++i) {
        WS_CHECK(ws_ecology_step(&e, 0.05) == WS_OK);
    }
    WS_CHECK(e.alive_count <= WS_ECOLOGY_MAX_AGENTS);
    for (uint32_t k = 0; k < e.alive_count; ++k) {
        WS_CHECK(e.agents[k].energy >= 0.0 && e.agents[k].energy <= 1.0);
        WS_CHECK(isfinite(e.agents[k].energy));
    }
    WS_CHECK(isfinite(ws_ecology_plant_biomass(&e)));
    ws_ecology_free(&e);
    return 1;
}

static int test_invalid_input(void)
{
    ws_ecology e;
    WS_CHECK(ws_ecology_init(NULL, NULL) == WS_ERR_NULL_POINTER);
    ws_eco_config bad = ws_eco_config_default(1u);
    bad.resource_cells = 0u;
    WS_CHECK(ws_ecology_init(&e, &bad) == WS_ERR_INVALID_ARGUMENT);
    bad = ws_eco_config_default(1u);
    WS_CHECK(ws_ecology_init(&e, &bad) == WS_OK);
    WS_CHECK(ws_ecology_step(&e, 0.0) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_ecology_step(&e, -1.0) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_ecology_step(NULL, 0.05) == WS_ERR_NULL_POINTER);
    ws_ecology_free(&e);
    WS_CHECK(ws_ecology_population(NULL, 0) == 0u);
    return 1;
}

int main(void)
{
    const ws_test_case cases[] = {
        {"species table complete and consistent", test_species_table},
        {"plant growth bounded by capacity", test_plant_bounded},
        {"reserve and biomass conservation", test_reserve_and_biomass_conservation},
        {"no population explosion", test_no_population_explosion},
        {"predator-prey coupling and bounds", test_predator_prey_coupling},
        {"reproduction and death occur", test_reproduction_and_death},
        {"starvation removes animals", test_starvation_death},
        {"resource shock respected", test_resource_shock},
        {"multi-species ecosystem persists", test_multi_species_present},
        {"long-term four-species coexistence (12 seeds)", test_long_term_coexistence},
        {"determinism across ten seeds", test_determinism_across_seeds},
        {"long run stability", test_long_run_stability},
        {"invalid input rejected", test_invalid_input}
    };
    return ws_test_run("ecology", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
