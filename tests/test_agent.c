/* Worldseed Engine — organism and behaviour tests (P6).
 *
 * Scientific regression suite from docs/07: homeostasis and reserve
 * conservation, need prioritisation, perception field-of-view and range,
 * threat-triggered flight, hunger- and thirst-driven foraging, rest recovery,
 * behaviour determinism (camera independence), a measurable learning-vs-control
 * comparison over multiple seeds, and invalid-input rejection. These exercise
 * the real models; there are no mocks.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "life/ws_agent.h"
#include "life/ws_organism.h"
#include "life/ws_perception.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static int all_finite_agent(const ws_agent *a)
{
    return isfinite(a->x) && isfinite(a->y) && isfinite(a->heading) &&
           isfinite(a->speed) && isfinite(a->org.energy) &&
           isfinite(a->org.hydration) && isfinite(a->org.body_temp_c);
}

static int test_organism_init_full(void)
{
    ws_organism o;
    ws_organism_init(&o, NULL);
    WS_CHECK(o.energy == 1.0 && o.hydration == 1.0);
    WS_CHECK(o.alive == 1 && o.health == 1.0);
    WS_CHECK(o.hunger == 0.0 && o.thirst == 0.0 && o.warmth == 0.0);
    WS_CHECK(fabs(o.body_temp_c - o.config.comfort_temp_c) < 1e-9);
    return 1;
}

static int test_reserve_conservation(void)
{
    ws_organism o;
    ws_organism_init(&o, NULL);
    double prev_e = o.energy;
    double prev_w = o.hydration;
    for (int i = 0; i < 20000; ++i) {
        ws_result r = ws_organism_step(&o, 0.02, 1.2, 20.0, 0);
        WS_CHECK(r == WS_OK);
        WS_CHECK(o.energy >= 0.0 && o.energy <= 1.0);
        WS_CHECK(o.hydration >= 0.0 && o.hydration <= 1.0);
        WS_CHECK(o.fatigue >= 0.0 && o.fatigue <= 1.0);
        WS_CHECK(o.health >= 0.0 && o.health <= 1.0);
        /* No free energy: reserves never rise without eating/drinking. */
        WS_CHECK(o.energy <= prev_e + 1e-12);
        WS_CHECK(o.hydration <= prev_w + 1e-12);
        prev_e = o.energy;
        prev_w = o.hydration;
    }
    return 1;
}

static int test_eat_drink_refill(void)
{
    ws_organism o;
    ws_organism_init(&o, NULL);
    /* Drain a little. */
    for (int i = 0; i < 2000; ++i) {
        (void)ws_organism_step(&o, 0.02, 0.0, 20.0, 0);
    }
    double e0 = o.energy;
    double absorbed = ws_organism_eat(&o, 0.1);
    WS_CHECK(absorbed > 0.0 && absorbed <= 0.1 + 1e-12);
    WS_CHECK(o.energy > e0);
    /* Eating past full absorbs only the room available. */
    double room = 1.0 - o.energy;
    double extra = ws_organism_eat(&o, 5.0);
    WS_CHECK(fabs(extra - room) < 1e-9);
    WS_CHECK(o.energy <= 1.0 + 1e-12);
    double d0 = o.hydration;
    (void)ws_organism_drink(&o, 0.05);
    WS_CHECK(o.hydration >= d0);
    WS_CHECK(o.hydration <= 1.0 + 1e-12);
    return 1;
}

static int test_death_when_starved(void)
{
    ws_organism o;
    ws_organism_init(&o, NULL);
    /* With no food or water the animal must run out and die; health is only
     * spent while a reserve is empty. */
    int died = 0;
    for (int i = 0; i < 200000 && !died; ++i) {
        (void)ws_organism_step(&o, 0.05, 1.0, 20.0, 0);
        if (!o.alive) {
            died = 1;
        }
    }
    WS_CHECK(died == 1);
    WS_CHECK(o.health == 0.0);
    WS_CHECK(o.energy == 0.0);
    return 1;
}

static int test_need_priority(void)
{
    ws_organism o;
    ws_organism_init(&o, NULL);
    /* Thirst drives hydration down fastest; make it the most urgent. */
    o.energy = 0.5;
    o.hydration = 0.1;
    o.fatigue = 0.2;
    o.body_temp_c = o.config.comfort_temp_c;
    /* Refresh derived drives via a zero-cost step. */
    (void)ws_organism_step(&o, 1e-9, 0.0, o.config.comfort_temp_c, 0);
    WS_CHECK(ws_organism_most_urgent(&o, 0.3) == WS_NEED_WATER);
    o.hydration = 1.0;
    o.energy = 0.05;
    (void)ws_organism_step(&o, 1e-9, 0.0, o.config.comfort_temp_c, 0);
    WS_CHECK(ws_organism_most_urgent(&o, 0.3) == WS_NEED_ENERGY);
    return 1;
}

static int test_perception_fov_and_range(void)
{
    ws_perception_config cfg = ws_perception_config_default();
    ws_perception p;
    ws_sense_point pts[3];
    memset(pts, 0, sizeof(pts));
    /* Food ahead, in range. */
    pts[0].kind = WS_SENSE_FOOD;
    pts[0].x = 10.0;
    pts[0].y = 0.0;
    pts[0].value = 0.8;
    /* Food behind (bearing ~pi), out of the forward field of view. */
    pts[1].kind = WS_SENSE_FOOD;
    pts[1].x = -5.0;
    pts[1].y = 0.0;
    pts[1].value = 1.0;
    /* Food ahead but beyond range. */
    pts[2].kind = WS_SENSE_FOOD;
    pts[2].x = cfg.range + 20.0;
    pts[2].y = 0.0;
    pts[2].value = 1.0;

    ws_perception_update(&p, &cfg, 0.0, 0.0, 0.0, pts, 3u, NULL);
    WS_CHECK(p.food.seen == 1);
    WS_CHECK(fabs(p.food.dist - 10.0) < 1e-9);
    WS_CHECK(fabs(p.food.bearing) < 1e-9);

    /* Turn around: the behind target is now ahead and nearer than the far one,
     * but the far one is beyond range; the near-behind one is chosen. */
    ws_perception_update(&p, &cfg, 0.0, 0.0, 3.14159265358979323846, pts, 3u, NULL);
    WS_CHECK(p.food.seen == 1);
    WS_CHECK(fabs(p.food.dist - 5.0) < 1e-9);
    return 1;
}

static int test_perception_skips_depleted(void)
{
    ws_perception_config cfg = ws_perception_config_default();
    ws_perception p;
    ws_sense_point pts[2];
    memset(pts, 0, sizeof(pts));
    pts[0].kind = WS_SENSE_FOOD;
    pts[0].x = 5.0;
    pts[0].value = 0.0; /* grazed out */
    pts[1].kind = WS_SENSE_FOOD;
    pts[1].x = 9.0;
    pts[1].value = 0.5;
    ws_perception_update(&p, &cfg, 0.0, 0.0, 0.0, pts, 2u, NULL);
    WS_CHECK(p.food.seen == 1);
    WS_CHECK(fabs(p.food.dist - 9.0) < 1e-9);
    return 1;
}

static int test_threat_triggers_flee(void)
{
    ws_agent a;
    ws_agent_config cfg = ws_agent_config_default();
    ws_agent_init(&a, &cfg, 12345u, 0.0, 0.0, 0.0);
    ws_sense_point pts[1];
    memset(pts, 0, sizeof(pts));
    pts[0].kind = WS_SENSE_THREAT;
    pts[0].x = 10.0;
    pts[0].y = 0.0;
    pts[0].value = 1.0;
    uint32_t n = 1u;
    double d0 = 10.0;
    for (int i = 0; i < 100; ++i) {
        WS_CHECK(ws_agent_step(&a, 0.02, pts, &n, 20.0) == WS_OK);
    }
    WS_CHECK(a.behavior == WS_BEHAVIOR_FLEE);
    double dx = a.x - pts[0].x;
    double dy = a.y - pts[0].y;
    double d1 = sqrt(dx * dx + dy * dy);
    WS_CHECK(d1 > d0); /* fled away from the threat */
    return 1;
}

static int test_forage_food_when_hungry(void)
{
    ws_agent a;
    ws_agent_config cfg = ws_agent_config_default();
    ws_agent_init(&a, &cfg, 777u, 0.0, 0.0, 0.0);
    a.org.energy = 0.25; /* hungry */
    ws_sense_point pts[1];
    memset(pts, 0, sizeof(pts));
    pts[0].kind = WS_SENSE_FOOD;
    pts[0].x = 12.0;
    pts[0].y = 0.0;
    pts[0].value = 1.0;
    uint32_t n = 1u;
    double e0 = a.org.energy;
    int saw_seek = 0;
    for (int i = 0; i < 3000; ++i) {
        WS_CHECK(ws_agent_step(&a, 0.02, pts, &n, 20.0) == WS_OK);
        if (a.behavior == WS_BEHAVIOR_SEEK_FOOD) {
            saw_seek = 1;
        }
    }
    WS_CHECK(saw_seek == 1);
    WS_CHECK(a.food_found > 0u);
    WS_CHECK(a.org.energy > e0);
    WS_CHECK(pts[0].value < 1.0); /* the patch was grazed */
    return 1;
}

static int test_forage_water_when_thirsty(void)
{
    ws_agent a;
    ws_agent_config cfg = ws_agent_config_default();
    ws_agent_init(&a, &cfg, 999u, 0.0, 0.0, 0.0);
    a.org.hydration = 0.2; /* thirsty */
    ws_sense_point pts[1];
    memset(pts, 0, sizeof(pts));
    pts[0].kind = WS_SENSE_WATER;
    pts[0].x = 10.0;
    pts[0].y = 0.0;
    pts[0].value = 1.0;
    uint32_t n = 1u;
    double w0 = a.org.hydration;
    for (int i = 0; i < 3000; ++i) {
        WS_CHECK(ws_agent_step(&a, 0.02, pts, &n, 20.0) == WS_OK);
    }
    WS_CHECK(a.water_found > 0u);
    WS_CHECK(a.org.hydration > w0);
    return 1;
}

static int test_rest_recovers_fatigue(void)
{
    ws_agent a;
    ws_agent_config cfg = ws_agent_config_default();
    ws_agent_init(&a, &cfg, 4242u, 0.0, 0.0, 0.0);
    a.org.fatigue = 0.8; /* most urgent need is rest */
    uint32_t n = 0u;
    double f0 = a.org.fatigue;
    for (int i = 0; i < 200; ++i) {
        WS_CHECK(ws_agent_step(&a, 0.02, NULL, &n, 20.0) == WS_OK);
    }
    WS_CHECK(a.behavior == WS_BEHAVIOR_REST);
    WS_CHECK(a.org.fatigue < f0);
    return 1;
}

static int test_behaviour_determinism(void)
{
    ws_agent a;
    ws_agent b;
    ws_agent_config cfg = ws_agent_config_default();
    ws_sense_point pts_a[3];
    ws_sense_point pts_b[3];
    memset(pts_a, 0, sizeof(pts_a));
    pts_a[0].kind = WS_SENSE_FOOD;
    pts_a[0].x = 15.0; pts_a[0].y = 4.0; pts_a[0].value = 1.0;
    pts_a[1].kind = WS_SENSE_WATER;
    pts_a[1].x = -8.0; pts_a[1].y = 6.0; pts_a[1].value = 1.0;
    pts_a[2].kind = WS_SENSE_THREAT;
    pts_a[2].x = 30.0; pts_a[2].y = -2.0; pts_a[2].value = 0.5;
    memcpy(pts_b, pts_a, sizeof(pts_a));
    ws_agent_init(&a, &cfg, 31337u, 0.0, 0.0, 0.3);
    ws_agent_init(&b, &cfg, 31337u, 0.0, 0.0, 0.3);
    a.org.energy = 0.4;
    b.org.energy = 0.4;
    uint32_t na = 3u;
    uint32_t nb = 3u;
    for (int i = 0; i < 4000; ++i) {
        WS_CHECK(ws_agent_step(&a, 0.02, pts_a, &na, 18.0) == WS_OK);
        WS_CHECK(ws_agent_step(&b, 0.02, pts_b, &nb, 18.0) == WS_OK);
        WS_CHECK(a.behavior == b.behavior);
    }
    WS_CHECK(fabs(a.x - b.x) < 1e-12);
    WS_CHECK(fabs(a.y - b.y) < 1e-12);
    WS_CHECK(fabs(a.heading - b.heading) < 1e-12);
    WS_CHECK(fabs(a.org.energy - b.org.energy) < 1e-12);
    WS_CHECK(a.food_found == b.food_found && a.water_found == b.water_found);
    WS_CHECK(all_finite_agent(&a));
    return 1;
}

/* One learning-vs-control episode: a single food patch that regrows on a fixed
 * interval. A learning agent remembers the patch location and returns to it when
 * hungry and it is not currently sensed; the control agent has memory disabled
 * and must re-find it by wandering. Returns the number of bites taken. */
static uint32_t forage_episode(uint64_t seed, int learning, double seconds)
{
    ws_agent_config cfg = ws_agent_config_default();
    cfg.learning_enabled = learning;
    ws_agent a;
    ws_agent_init(&a, &cfg, seed, 0.0, 0.0, 0.0);
    a.org.energy = 0.30;

    ws_sense_point pts[1];
    memset(pts, 0, sizeof(pts));
    pts[0].kind = WS_SENSE_FOOD;
    pts[0].x = 20.0;
    pts[0].y = 0.0;
    pts[0].value = 1.0;
    uint32_t n = 1u;

    int steps = (int)(seconds / 0.02);
    int regrow = (int)(5.0 / 0.02);
    for (int i = 0; i < steps; ++i) {
        if (i % regrow == 0) {
            pts[0].value = 1.0; /* world layer regrows the patch */
        }
        (void)ws_agent_step(&a, 0.02, pts, &n, 20.0);
    }
    return a.food_found;
}

static int test_learning_vs_control(void)
{
    /* Across many seeds the learning agent must out-forage the memoryless
     * control; a single seed is never used as acceptance (docs/07). */
    int wins = 0;
    uint32_t learn_total = 0u;
    uint32_t control_total = 0u;
    for (uint64_t s = 0; s < 10u; ++s) {
        uint32_t lr = forage_episode(1000u + s * 7u, 1, 90.0);
        uint32_t ct = forage_episode(1000u + s * 7u, 0, 90.0);
        learn_total += lr;
        control_total += ct;
        if (lr > ct) {
            wins += 1;
        }
    }
    WS_CHECK(learn_total > control_total);
    WS_CHECK(wins >= 6); /* wins on a clear majority of seeds */
    return 1;
}

static int test_invalid_input(void)
{
    ws_agent a;
    ws_agent_init(&a, NULL, 1u, 0.0, 0.0, 0.0);
    uint32_t n = 0u;
    WS_CHECK(ws_agent_step(&a, 0.0, NULL, &n, 20.0) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_agent_step(&a, -0.1, NULL, &n, 20.0) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_agent_step(NULL, 0.02, NULL, &n, 20.0) == WS_ERR_NULL_POINTER);
    ws_organism o;
    ws_organism_init(&o, NULL);
    WS_CHECK(ws_organism_step(&o, 0.0, 1.0, 20.0, 0) == WS_ERR_INVALID_ARGUMENT);
    return 1;
}

int main(void)
{
    const ws_test_case cases[] = {
        {"organism init full reserves", test_organism_init_full},
        {"reserve conservation (no free energy)", test_reserve_conservation},
        {"eat and drink refill reserves", test_eat_drink_refill},
        {"death when reserves exhausted", test_death_when_starved},
        {"need prioritisation", test_need_priority},
        {"perception field of view and range", test_perception_fov_and_range},
        {"perception skips depleted patches", test_perception_skips_depleted},
        {"threat triggers flight", test_threat_triggers_flee},
        {"forage for food when hungry", test_forage_food_when_hungry},
        {"forage for water when thirsty", test_forage_water_when_thirsty},
        {"rest recovers fatigue", test_rest_recovers_fatigue},
        {"behaviour is deterministic (camera independent)", test_behaviour_determinism},
        {"learning beats memoryless control across seeds", test_learning_vs_control},
        {"invalid input rejected", test_invalid_input}
    };
    return ws_test_run("agent", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
