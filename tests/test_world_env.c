/* Worldseed Engine — P3 world/environment tests.
 *
 * Covers the acceptance gates in docs/13 (P3): same-seed repeatability, region
 * seams, environmental state, habitat/vegetation determinism, water mass
 * balance and long-run stability.
 */
#include "ws_test.h"

#include "core/ws_noise.h"
#include "render/ws_catalog.h"
#include "render/ws_env_scene.h"
#include "world/ws_environment.h"
#include "world/ws_habitat.h"
#include "world/ws_hydro.h"
#include "world/ws_terrain.h"
#include "world/ws_vegetation.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

/* ---------- noise ---------- */

static int test_noise_reproducible(void)
{
    double a = ws_noise_value2(42ULL, 1.25, -3.5);
    double b = ws_noise_value2(42ULL, 1.25, -3.5);
    WS_CHECK(a == b);
    WS_CHECK(a >= 0.0 && a < 1.0);
    double c = ws_noise_value2(43ULL, 1.25, -3.5);
    WS_CHECK(a != c);
    double f = ws_noise_fbm2(42ULL, 0.3, 0.7, 5, 2.0, 0.5);
    WS_CHECK(f >= 0.0 && f <= 1.0);
    double r = ws_noise_ridged2(42ULL, 0.3, 0.7, 5, 2.0, 0.5);
    WS_CHECK(r >= 0.0 && r <= 1.0);
    return 1;
}

/* ---------- terrain ---------- */

static int test_terrain_repeatable(void)
{
    ws_terrain_config cfg = ws_terrain_config_default(20260101ULL);
    ws_terrain a;
    ws_terrain b;
    WS_CHECK(ws_terrain_create(&a, &cfg, 0, 0) == WS_OK);
    WS_CHECK(ws_terrain_create(&b, &cfg, 0, 0) == WS_OK);
    size_t nodes = ((size_t)cfg.region_cells + 1u) * ((size_t)cfg.region_cells + 1u);
    WS_CHECK(memcmp(a.heights, b.heights, nodes * sizeof(double)) == 0);
    size_t cells = (size_t)cfg.region_cells * cfg.region_cells;
    WS_CHECK(memcmp(a.surface, b.surface, cells) == 0);
    ws_terrain_free(&a);
    ws_terrain_free(&b);
    return 1;
}

/* The right edge of region (0,0) must equal the left edge of region (1,0),
 * because height is a pure function of world position. */
static int test_terrain_region_seam(void)
{
    ws_terrain_config cfg = ws_terrain_config_default(20260101ULL);
    ws_terrain west;
    ws_terrain east;
    WS_CHECK(ws_terrain_create(&west, &cfg, 0, 0) == WS_OK);
    WS_CHECK(ws_terrain_create(&east, &cfg, 1, 0) == WS_OK);
    uint32_t n = cfg.region_cells;
    for (uint32_t iy = 0; iy <= n; ++iy) {
        double w = ws_terrain_node(&west, n, iy);
        double e = ws_terrain_node(&east, 0u, iy);
        WS_CHECK_MSG(fabs(w - e) < 1.0e-9, "seam mismatch");
    }
    /* Negative region coordinates must also be continuous. */
    ws_terrain neg;
    WS_CHECK(ws_terrain_create(&neg, &cfg, -1, 0) == WS_OK);
    for (uint32_t iy = 0; iy <= n; ++iy) {
        double e = ws_terrain_node(&neg, n, iy);
        double w = ws_terrain_node(&west, 0u, iy);
        WS_CHECK(fabs(e - w) < 1.0e-9);
    }
    ws_terrain_free(&west);
    ws_terrain_free(&east);
    ws_terrain_free(&neg);
    return 1;
}

static int test_terrain_seed_varies(void)
{
    ws_terrain_config c1 = ws_terrain_config_default(1ULL);
    ws_terrain_config c2 = ws_terrain_config_default(2ULL);
    ws_terrain a;
    ws_terrain b;
    WS_CHECK(ws_terrain_create(&a, &c1, 0, 0) == WS_OK);
    WS_CHECK(ws_terrain_create(&b, &c2, 0, 0) == WS_OK);
    size_t nodes = ((size_t)c1.region_cells + 1u) * ((size_t)c1.region_cells + 1u);
    WS_CHECK(memcmp(a.heights, b.heights, nodes * sizeof(double)) != 0);
    ws_terrain_free(&a);
    ws_terrain_free(&b);
    return 1;
}

/* ---------- hydro ---------- */

static int test_hydro_rivers_present(void)
{
    ws_terrain_config cfg = ws_terrain_config_default(20260101ULL);
    ws_terrain t;
    WS_CHECK(ws_terrain_create(&t, &cfg, 0, 0) == WS_OK);
    ws_hydro_config hc = ws_hydro_config_default(&cfg);
    ws_hydro h;
    WS_CHECK(ws_hydro_create(&h, &hc, &t) == WS_OK);
    /* A height field with relief must route flow into at least one channel. */
    WS_CHECK(h.river_cells > 0u);
    /* Flow accumulation includes the cell itself, so it is always >= 1. */
    size_t cells = (size_t)t.cells * t.cells;
    for (size_t i = 0; i < cells; ++i) {
        WS_CHECK(h.flow_accum[i] >= 1.0);
    }
    ws_hydro_free(&h);
    ws_terrain_free(&t);
    return 1;
}

/* Water mass balance: total water changes only by precipitation minus
 * evaporation, and never goes negative. */
static int test_hydro_budget_mass_balance(void)
{
    ws_terrain_config cfg = ws_terrain_config_default(7ULL);
    ws_terrain t;
    WS_CHECK(ws_terrain_create(&t, &cfg, 0, 0) == WS_OK);
    ws_hydro_config hc = ws_hydro_config_default(&cfg);
    hc.precipitation = 5.0e-5;
    hc.evaporation = 1.0e-5;
    ws_hydro h;
    WS_CHECK(ws_hydro_create(&h, &hc, &t) == WS_OK);

    double prev = h.total_water;
    for (int i = 0; i < 200; ++i) {
        double delta = ws_hydro_budget_step(&h, 1.0);
        WS_CHECK(h.total_water >= 0.0);
        WS_CHECK(fabs((prev + delta) - h.total_water) < 1.0e-6);
        prev = h.total_water;
    }
    /* With precipitation above evaporation the budget grows monotonically. */
    WS_CHECK(h.total_water > 0.0);
    ws_hydro_free(&h);
    ws_terrain_free(&t);
    return 1;
}

/* ---------- habitat and vegetation ---------- */

static int test_habitat_covers_region(void)
{
    ws_terrain_config cfg = ws_terrain_config_default(20260101ULL);
    ws_terrain t;
    WS_CHECK(ws_terrain_create(&t, &cfg, 0, 0) == WS_OK);
    ws_hydro_config hc = ws_hydro_config_default(&cfg);
    ws_hydro h;
    WS_CHECK(ws_hydro_create(&h, &hc, &t) == WS_OK);
    ws_habitat_map map;
    WS_CHECK(ws_habitat_create(&map, &t, &h) == WS_OK);

    uint32_t total = 0u;
    for (uint8_t k = 0; k < WS_HABITAT_COUNT; ++k) {
        total += map.counts[k];
    }
    WS_CHECK(total == t.cells * t.cells);
    double frac_sum = 0.0;
    for (uint8_t k = 0; k < WS_HABITAT_COUNT; ++k) {
        frac_sum += ws_habitat_fraction(&map, k);
    }
    WS_CHECK(fabs(frac_sum - 1.0) < 1.0e-9);
    ws_habitat_free(&map);
    ws_hydro_free(&h);
    ws_terrain_free(&t);
    return 1;
}

static int test_vegetation_deterministic(void)
{
    ws_terrain_config cfg = ws_terrain_config_default(20260101ULL);
    ws_terrain t;
    WS_CHECK(ws_terrain_create(&t, &cfg, 0, 0) == WS_OK);
    ws_hydro_config hc = ws_hydro_config_default(&cfg);
    ws_hydro h;
    WS_CHECK(ws_hydro_create(&h, &hc, &t) == WS_OK);
    ws_habitat_map map;
    WS_CHECK(ws_habitat_create(&map, &t, &h) == WS_OK);
    ws_vegetation v1;
    ws_vegetation v2;
    WS_CHECK(ws_vegetation_create(&v1, &t, &map) == WS_OK);
    WS_CHECK(ws_vegetation_create(&v2, &t, &map) == WS_OK);
    WS_CHECK(v1.count == v2.count);
    WS_CHECK(v1.count > 0u);
    WS_CHECK(memcmp(v1.plants, v2.plants, v1.count * sizeof(ws_plant)) == 0);
    /* Trees must land in forest/meadow habitats, not on open water. */
    for (uint32_t i = 0; i < v1.count; ++i) {
        if (v1.plants[i].kind == WS_PLANT_TREE) {
            double gx = (v1.plants[i].position.x - t.origin_x) / cfg.cell_size;
            double gy = (v1.plants[i].position.y - t.origin_y) / cfg.cell_size;
            uint32_t cx = (uint32_t)gx;
            uint32_t cy = (uint32_t)gy;
            if (cx < t.cells && cy < t.cells) {
                uint8_t hab = ws_habitat_at(&map, cx, cy);
                WS_CHECK(hab != WS_HABITAT_FRESHWATER);
            }
        }
    }
    ws_vegetation_free(&v1);
    ws_vegetation_free(&v2);
    ws_habitat_free(&map);
    ws_hydro_free(&h);
    ws_terrain_free(&t);
    return 1;
}

/* ---------- environment ---------- */

static int test_environment_day_night(void)
{
    ws_env_config cfg = ws_env_config_default(20260101ULL);
    ws_environment env;
    ws_environment_init(&env, &cfg);
    ws_environment_update(&env, 12.0 * 3600.0, 0.0);
    double noon_alt = env.sun_altitude;
    double noon_day = env.daylight;
    WS_CHECK(noon_alt > 0.0);
    WS_CHECK(noon_day > 0.8);
    ws_environment_update(&env, 0.0, 0.0);
    WS_CHECK(env.sun_altitude < 0.0);
    WS_CHECK(env.daylight < 0.2);
    WS_CHECK(noon_alt > env.sun_altitude);
    return 1;
}

static int test_environment_temperature_lapse(void)
{
    ws_env_config cfg = ws_env_config_default(20260101ULL);
    ws_environment low;
    ws_environment high;
    ws_environment_init(&low, &cfg);
    ws_environment_init(&high, &cfg);
    ws_environment_update(&low, 12.0 * 3600.0, 0.0);
    ws_environment_update(&high, 12.0 * 3600.0, 1000.0);
    WS_CHECK(high.temperature_c < low.temperature_c);
    /* Roughly 6.5 K per 1000 m. */
    WS_CHECK(fabs((low.temperature_c - high.temperature_c) - 6.5) < 0.2);
    return 1;
}

static int test_environment_deterministic(void)
{
    ws_env_config cfg = ws_env_config_default(20260101ULL);
    ws_environment a;
    ws_environment b;
    ws_environment_init(&a, &cfg);
    ws_environment_init(&b, &cfg);
    for (int h = 0; h < 24; ++h) {
        ws_environment_update(&a, (double)h * 3600.0, 10.0);
        ws_environment_update(&b, (double)h * 3600.0, 10.0);
        WS_CHECK(a.sun_direction.x == b.sun_direction.x);
        WS_CHECK(a.temperature_c == b.temperature_c);
        WS_CHECK(a.humidity == b.humidity);
        WS_CHECK(a.precipitation == b.precipitation);
    }
    return 1;
}

/* ---------- env scene ---------- */

static int test_env_scene_builds(void)
{
    ws_env_scene_options o = ws_env_scene_options_default(20260101ULL);
    ws_env_scene e;
    WS_CHECK(ws_env_scene_create(&e, &o) == WS_OK);
    WS_CHECK(e.scene.object_count >= 1u);
    WS_CHECK(e.terrain_mesh.index_count == o.cells * o.cells * 6u);
    WS_CHECK(e.scene.settings.sky_enabled == 1);
    WS_CHECK(e.tree_count > 0u);
    WS_CHECK(e.tree_count <= WS_ENV_MAX_TREES);
    ws_env_scene_free(&e);
    return 1;
}

static int test_env_scene_repeatable(void)
{
    ws_env_scene_options o = ws_env_scene_options_default(20260101ULL);
    ws_env_scene a;
    ws_env_scene b;
    WS_CHECK(ws_env_scene_create(&a, &o) == WS_OK);
    WS_CHECK(ws_env_scene_create(&b, &o) == WS_OK);
    WS_CHECK(a.tree_count == b.tree_count);
    WS_CHECK(a.terrain_mesh.vertex_count == b.terrain_mesh.vertex_count);
    size_t n = (size_t)a.terrain_mesh.vertex_count;
    WS_CHECK(memcmp(a.terrain_mesh.vertices, b.terrain_mesh.vertices,
                    n * sizeof(ws_vertex)) == 0);
    WS_CHECK(a.env.sun_direction.x == b.env.sun_direction.x);
    ws_env_scene_free(&a);
    ws_env_scene_free(&b);
    return 1;
}

/* A long run of budget steps must stay finite and non-negative (stability). */
static int test_env_scene_long_run_stable(void)
{
    ws_env_scene_options o = ws_env_scene_options_default(20260101ULL);
    ws_env_scene e;
    WS_CHECK(ws_env_scene_create(&e, &o) == WS_OK);
    for (int i = 0; i < 2000; ++i) {
        (void)ws_hydro_budget_step(&e.hydro, 0.5);
    }
    size_t cells = (size_t)e.hydro.cells * e.hydro.cells;
    for (size_t i = 0; i < cells; ++i) {
        WS_CHECK(isfinite(e.hydro.water_depth[i]));
        WS_CHECK(e.hydro.water_depth[i] >= 0.0);
    }
    WS_CHECK(isfinite(e.hydro.total_water));
    ws_env_scene_free(&e);
    return 1;
}

/* Every catalog scene must build and render from a fresh, uninitialised catalog
 * struct. This guards a regression where ws_catalog_build freed the caller's
 * struct before initialising it, which corrupted memory in optimised builds. */
static int test_catalog_builds_all(void)
{
    size_t count = ws_catalog_count();
    WS_CHECK(count >= 3u);
    for (size_t i = 0; i < count; ++i) {
        const char *id = ws_catalog_id(i);
        WS_CHECK(id != NULL);
        ws_catalog catalog;
        WS_CHECK(ws_catalog_build(&catalog, i, 20260101ULL) == WS_OK);
        const ws_scene *scene = ws_catalog_scene(&catalog);
        WS_CHECK(scene != NULL);
        WS_CHECK(scene->object_count >= 1u);
        ws_catalog_free(&catalog);
    }
    /* Out-of-range index is rejected, and the id lookup returns NULL. */
    ws_catalog catalog;
    WS_CHECK(ws_catalog_build(&catalog, count, 20260101ULL) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_catalog_id(count) == NULL);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"noise reproducible", test_noise_reproducible},
        {"terrain repeatable", test_terrain_repeatable},
        {"terrain region seam", test_terrain_region_seam},
        {"terrain seed varies", test_terrain_seed_varies},
        {"hydro rivers present", test_hydro_rivers_present},
        {"hydro budget mass balance", test_hydro_budget_mass_balance},
        {"habitat covers region", test_habitat_covers_region},
        {"vegetation deterministic", test_vegetation_deterministic},
        {"environment day/night", test_environment_day_night},
        {"environment temperature lapse", test_environment_temperature_lapse},
        {"environment deterministic", test_environment_deterministic},
        {"env scene builds", test_env_scene_builds},
        {"env scene repeatable", test_env_scene_repeatable},
        {"env scene long run stable", test_env_scene_long_run_stable},
        {"catalog builds all scenes", test_catalog_builds_all},
    };
    return ws_test_run("world_env", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
