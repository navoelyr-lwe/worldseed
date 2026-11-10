/* Worldseed Engine — persistent, multi-scale world tests (P8).
 *
 * Regression suite from docs/13 P8 acceptance: region identity and boundaries,
 * off-camera progression, unload/load without state loss, near/mid/far level
 * assignment, checkpoint save/load (including a damaged-record case), journal
 * integrity, and replay equality by SHA-256 digest. Real models; no mocks.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "life/ws_population.h"
#include "world/ws_region.h"
#include "world/ws_world_host.h"

#include <math.h>
#include <string.h>

static int test_region_geometry(void)
{
    ws_region_config c = ws_region_config_default(42u);
    WS_CHECK(ws_region_extent(&c) > 0.0);

    /* Origin and negative coordinates both map cleanly. */
    ws_region_pos p = ws_region_from_world(&c, 10.0, 10.0);
    WS_CHECK(p.region.x == 0 && p.region.y == 0);
    WS_CHECK(p.local_x >= 0.0 && p.local_x < ws_region_extent(&c));

    p = ws_region_from_world(&c, -1.0, -1.0);
    WS_CHECK(p.region.x == -1 && p.region.y == -1);
    WS_CHECK(p.local_x >= 0.0 && p.local_x < ws_region_extent(&c));

    /* Round-trip world <-> region-local. */
    double wx = 0.0, wy = 0.0;
    ws_region_to_world(&c, ws_region_coord_make(-3, 5), 12.5, 7.25, &wx, &wy);
    ws_region_pos back = ws_region_from_world(&c, wx, wy);
    WS_CHECK(back.region.x == -3 && back.region.y == 5);
    WS_CHECK(fabs(back.local_x - 12.5) < 1e-6);
    WS_CHECK(fabs(back.local_y - 7.25) < 1e-6);

    /* Adjacency and distance. */
    ws_region_coord a = ws_region_coord_make(2, 2);
    WS_CHECK(ws_region_adjacent(a, ws_region_coord_make(3, 2)));
    WS_CHECK(ws_region_adjacent(a, ws_region_coord_make(2, 1)));
    WS_CHECK(!ws_region_adjacent(a, ws_region_coord_make(3, 3))); /* diagonal */
    WS_CHECK(!ws_region_adjacent(a, a));
    WS_CHECK(ws_region_distance(a, ws_region_coord_make(-1, 2)) == 3);
    WS_CHECK(ws_region_distance(a, ws_region_coord_make(2, 5)) == 3);

    /* Region identity is a pure function of seed and coordinates. */
    uint64_t h1 = ws_region_hash(&c, a);
    uint64_t h2 = ws_region_hash(&c, a);
    WS_CHECK(h1 == h2);
    WS_CHECK(h1 != ws_region_hash(&c, ws_region_coord_make(3, 2)));
    ws_region_config c2 = ws_region_config_default(43u);
    WS_CHECK(h1 != ws_region_hash(&c2, a));
    return 1;
}

static int test_population_bounded(void)
{
    ws_pop_config cfg = ws_pop_config_default(7u);
    ws_pop_state s = ws_pop_seed(&cfg, 0x1234u);
    double cap = ws_pop_plant_capacity(&cfg);
    for (int i = 0; i < 200000; ++i) {
        WS_CHECK(ws_pop_step(&cfg, &s, 0.25) == WS_OK);
        WS_CHECK(isfinite(s.plant) && s.plant >= 0.0 && s.plant <= cap * 1.000001);
        for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
            WS_CHECK(isfinite(s.count[k]));
            WS_CHECK(s.count[k] >= 0.0);
            WS_CHECK(s.count[k] <= cfg.count_cap[k]);
        }
    }
    /* Something is alive at the end (the model is not trivially extinct). */
    WS_CHECK(ws_pop_total(&s) > 0.0);
    return 1;
}

static int test_population_predator_prey(void)
{
    /* With no rabbits the fox cannot persist; with rabbits it can. This checks
     * the food-web coupling rather than a hard-coded number. */
    ws_pop_config cfg = ws_pop_config_default(3u);
    ws_pop_state s = ws_pop_seed(&cfg, 0x55u);
    s.count[WS_SPECIES_RABBIT] = 0.0;
    s.count[WS_SPECIES_FOX] = 10.0;
    for (int i = 0; i < 40000; ++i) {
        WS_CHECK(ws_pop_step(&cfg, &s, 0.25) == WS_OK);
    }
    WS_CHECK(s.count[WS_SPECIES_FOX] < 10.0); /* declined without prey */

    ws_pop_state s2 = ws_pop_seed(&cfg, 0x55u);
    s2.count[WS_SPECIES_RABBIT] = 80.0;
    s2.count[WS_SPECIES_FOX] = 2.0;
    for (int i = 0; i < 40000; ++i) {
        WS_CHECK(ws_pop_step(&cfg, &s2, 0.25) == WS_OK);
    }
    WS_CHECK(s2.count[WS_SPECIES_FOX] > 0.0);
    return 1;
}

static int test_host_loaded_set_and_levels(void)
{
    ws_world_host_config cfg = ws_world_host_config_default(1u, 1u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);

    /* At focus (0,0) the loaded set is the (2*radius_load+1)^2 square. */
    int64_t r = (int64_t)cfg.radius_load;
    uint32_t expected = (uint32_t)((2 * r + 1) * (2 * r + 1));
    WS_CHECK(ws_world_host_loaded_count(&host) == expected);
    WS_CHECK(ws_world_host_level(&host, ws_region_coord_make(0, 0)) == WS_LEVEL_NEAR);
    WS_CHECK(ws_world_host_level(&host, ws_region_coord_make(1, 1)) == WS_LEVEL_NEAR);
    WS_CHECK(ws_world_host_level(&host, ws_region_coord_make(2, 0)) == WS_LEVEL_MID);
    WS_CHECK(ws_world_host_level(&host, ws_region_coord_make(3, 0)) == WS_LEVEL_FAR);
    WS_CHECK(ws_world_host_level(&host, ws_region_coord_make(4, 0)) == WS_LEVEL_UNLOADED);
    return 1;
}

static int test_offcamera_progression(void)
{
    /* A region far from the focus still advances, and the focus region keeps
     * progressing while the camera (focus) is somewhere else. */
    ws_world_host_config cfg = ws_world_host_config_default(2u, 2u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);

    ws_region_coord far = ws_region_coord_make((int64_t)cfg.radius_load, 0);
    const ws_region_slot *before = ws_world_host_find(&host, far);
    WS_CHECK(before != NULL);
    double plant0 = before->pop.plant;
    double time0 = before->pop.time_s;

    WS_CHECK(ws_world_host_run(&host, 600.0) == WS_OK);

    const ws_region_slot *after = ws_world_host_find(&host, far);
    WS_CHECK(after != NULL);
    WS_CHECK(after->pop.time_s > time0);
    WS_CHECK(fabs(after->pop.plant - plant0) > 1e-9); /* it changed */

    /* The focus region advanced every tick (NEAR cadence = 1 s). */
    const ws_region_slot *focus = ws_world_host_find(&host, ws_region_coord_make(0, 0));
    WS_CHECK(focus != NULL);
    WS_CHECK(focus->pop.time_s >= 599.0);
    return 1;
}

static int test_unload_load_no_state_loss(void)
{
    /* Load a region, let it evolve, move the focus away so it unloads, then move
     * back and confirm the evolved state is restored (not reseeded). */
    ws_world_host_config cfg = ws_world_host_config_default(4u, 4u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);
    WS_CHECK(ws_world_host_run(&host, 400.0) == WS_OK);

    ws_region_coord probe = ws_region_coord_make(0, 0);
    const ws_region_slot *s0 = ws_world_host_find(&host, probe);
    WS_CHECK(s0 != NULL);
    double plant_evolved = s0->pop.plant;
    double time_evolved = s0->pop.time_s;
    double deer_evolved = s0->pop.count[WS_SPECIES_DEER];

    /* Move far enough away that the probe leaves the loaded radius. */
    int64_t jump = (int64_t)cfg.radius_load * 3 + 1;
    WS_CHECK(ws_world_host_set_focus(&host, ws_region_coord_make(jump, 0)) == WS_OK);
    WS_CHECK(ws_world_host_level(&host, probe) == WS_LEVEL_UNLOADED);
    WS_CHECK(ws_world_host_find(&host, probe) == NULL);

    /* Return: the region is loaded again with its evolved state intact. */
    WS_CHECK(ws_world_host_set_focus(&host, probe) == WS_OK);
    const ws_region_slot *s1 = ws_world_host_find(&host, probe);
    WS_CHECK(s1 != NULL);
    WS_CHECK(fabs(s1->pop.plant - plant_evolved) < 1e-9);
    WS_CHECK(fabs(s1->pop.time_s - time_evolved) < 1e-9);
    WS_CHECK(fabs(s1->pop.count[WS_SPECIES_DEER] - deer_evolved) < 1e-9);
    return 1;
}

static int test_streaming_bounds(void)
{
    /* Streaming never exceeds the loaded capacity and the loaded count tracks
     * the (2r+1)^2 square at every focus. */
    ws_world_host_config cfg = ws_world_host_config_default(9u, 9u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);
    int64_t r = (int64_t)cfg.radius_load;
    uint32_t expected = (uint32_t)((2 * r + 1) * (2 * r + 1));
    for (int i = 0; i < 40; ++i) {
        int64_t fx = (int64_t)(i % 7) - 3;
        int64_t fy = (int64_t)(i % 5) - 2;
        WS_CHECK(ws_world_host_set_focus(&host, ws_region_coord_make(fx, fy)) == WS_OK);
        WS_CHECK(ws_world_host_loaded_count(&host) <= WS_HOST_MAX_LOADED);
        WS_CHECK(ws_world_host_loaded_count(&host) == expected);
        WS_CHECK(ws_world_host_run(&host, 8.0) == WS_OK);
    }
    WS_CHECK(host.total_loads > 0u);
    WS_CHECK(host.total_unloads > 0u);
    return 1;
}

static int test_journal_integrity(void)
{
    ws_world_host_config cfg = ws_world_host_config_default(5u, 5u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);
    WS_CHECK(ws_world_host_run(&host, 50.0) == WS_OK);
    WS_CHECK(ws_world_host_set_focus(&host, ws_region_coord_make(3, 0)) == WS_OK);

    uint32_t n = ws_world_host_event_count(&host);
    WS_CHECK(n > 0u);
    uint64_t prev_seq = 0u;
    int saw_load = 0, saw_unload = 0, saw_focus = 0;
    for (uint32_t i = 0; i < n; ++i) {
        ws_event ev;
        WS_CHECK(ws_world_host_event_at(&host, i, &ev) == WS_OK);
        WS_CHECK(ev.sequence > prev_seq); /* strictly increasing */
        prev_seq = ev.sequence;
        WS_CHECK(ev.type == WS_EVENT_CUSTOM);
        uint8_t kind = ev.payload[0];
        if (kind == WS_HOST_EV_REGION_LOADED) saw_load = 1;
        if (kind == WS_HOST_EV_REGION_UNLOADED) saw_unload = 1;
        if (kind == WS_HOST_EV_FOCUS_MOVED) saw_focus = 1;
        WS_CHECK(ws_host_event_kind_name(kind) != NULL);
    }
    WS_CHECK(saw_load && saw_unload && saw_focus);
    /* Out-of-range index is rejected. */
    ws_event ev;
    WS_CHECK(ws_world_host_event_at(&host, n, &ev) == WS_ERR_NOT_FOUND);
    return 1;
}

static int test_checkpoint_roundtrip(void)
{
    ws_world_host_config cfg = ws_world_host_config_default(6u, 6u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);
    WS_CHECK(ws_world_host_set_focus(&host, ws_region_coord_make(2, -1)) == WS_OK);
    WS_CHECK(ws_world_host_run(&host, 300.0) == WS_OK);

    uint8_t buf[64u * 1024u];
    size_t len = 0u;
    WS_CHECK(ws_world_host_save(&host, buf, sizeof(buf), &len) == WS_OK);
    WS_CHECK(len > 0u);

    char d0[WS_HOST_DIGEST_HEX];
    WS_CHECK(ws_world_host_digest(&host, d0) == WS_OK);

    ws_world_host restored;
    WS_CHECK(ws_world_host_load(&restored, buf, len) == WS_OK);

    char d1[WS_HOST_DIGEST_HEX];
    WS_CHECK(ws_world_host_digest(&restored, d1) == WS_OK);
    WS_CHECK(strcmp(d0, d1) == 0);

    /* The restored host continues identically. */
    WS_CHECK(ws_world_host_run(&host, 120.0) == WS_OK);
    WS_CHECK(ws_world_host_run(&restored, 120.0) == WS_OK);
    char d2[WS_HOST_DIGEST_HEX], d3[WS_HOST_DIGEST_HEX];
    WS_CHECK(ws_world_host_digest(&host, d2) == WS_OK);
    WS_CHECK(ws_world_host_digest(&restored, d3) == WS_OK);
    WS_CHECK(strcmp(d2, d3) == 0);

    /* Too-small buffer is reported, not truncated silently. */
    size_t small = 0u;
    WS_CHECK(ws_world_host_save(&host, buf, 8u, &small) == WS_ERR_CAPACITY);

    /* A damaged record is rejected. */
    uint8_t damaged[64u * 1024u];
    memcpy(damaged, buf, len);
    damaged[0] ^= 0xFFu; /* corrupt the version field */
    ws_world_host bad;
    WS_CHECK(ws_world_host_load(&bad, damaged, len) != WS_OK);
    /* A truncated record is rejected. */
    WS_CHECK(ws_world_host_load(&bad, buf, 4u) != WS_OK);
    return 1;
}

static int test_replay_determinism(void)
{
    /* The same config plus the same focus path yields the same digest. */
    ws_world_host_config cfg = ws_world_host_config_default(0xABCDu, 0x1234u);
    ws_region_coord path[5] = {
        {0, 0}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}
    };
    char a[WS_HOST_DIGEST_HEX], b[WS_HOST_DIGEST_HEX];
    WS_CHECK(ws_world_host_replay(&cfg, path, 5u, 120.0, 60.0, a) == WS_OK);
    WS_CHECK(ws_world_host_replay(&cfg, path, 5u, 120.0, 60.0, b) == WS_OK);
    WS_CHECK(strcmp(a, b) == 0);

    /* A different seed changes the outcome (the test is not vacuous). */
    ws_world_host_config cfg2 = ws_world_host_config_default(0xABCEu, 0x1234u);
    char c[WS_HOST_DIGEST_HEX];
    WS_CHECK(ws_world_host_replay(&cfg2, path, 5u, 120.0, 60.0, c) == WS_OK);
    WS_CHECK(strcmp(a, c) != 0);

    /* A different focus path changes the outcome. */
    ws_region_coord path2[5] = {
        {0, 0}, {2, 0}, {2, 2}, {0, 2}, {-2, 0}
    };
    char d[WS_HOST_DIGEST_HEX];
    WS_CHECK(ws_world_host_replay(&cfg, path2, 5u, 120.0, 60.0, d) == WS_OK);
    WS_CHECK(strcmp(a, d) != 0);
    return 1;
}

static int test_replay_across_seeds(void)
{
    /* Ten fixed seeds all replay reproducibly. Single-seed acceptance is not
     * allowed (docs/07), so the determinism check spans many seeds. */
    ws_region_coord path[4] = { {0, 0}, {1, 0}, {1, 1}, {0, 1} };
    for (uint64_t s = 1u; s <= 10u; ++s) {
        ws_world_host_config cfg = ws_world_host_config_default(s, s);
        char a[WS_HOST_DIGEST_HEX], b[WS_HOST_DIGEST_HEX];
        WS_CHECK(ws_world_host_replay(&cfg, path, 4u, 90.0, 30.0, a) == WS_OK);
        WS_CHECK(ws_world_host_replay(&cfg, path, 4u, 90.0, 30.0, b) == WS_OK);
        WS_CHECK(strcmp(a, b) == 0);
    }
    return 1;
}

static int test_long_run_stability(void)
{
    /* A long simulated run keeps every quantity finite and bounded. */
    ws_world_host_config cfg = ws_world_host_config_default(11u, 11u);
    ws_world_host host;
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);
    ws_region_coord path[6] = { {0,0},{1,0},{2,0},{2,1},{1,1},{0,1} };
    for (uint32_t i = 0; i < 6u; ++i) {
        WS_CHECK(ws_world_host_set_focus(&host, path[i]) == WS_OK);
        WS_CHECK(ws_world_host_run(&host, 600.0) == WS_OK);
        for (uint32_t j = 0; j < ws_world_host_loaded_count(&host); ++j) {
            const ws_region_slot *s = &host.slots[j];
            WS_CHECK(isfinite(s->pop.plant) && s->pop.plant >= 0.0);
            for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
                WS_CHECK(isfinite(s->pop.count[k]) && s->pop.count[k] >= 0.0);
                WS_CHECK(s->pop.count[k] <= cfg.pop.count_cap[k]);
            }
        }
    }
    WS_CHECK(host.time_s >= 3600.0);
    return 1;
}

static int test_invalid_input(void)
{
    ws_world_host host;
    WS_CHECK(ws_world_host_init(NULL, NULL) == WS_ERR_NULL_POINTER);
    ws_world_host_config bad = ws_world_host_config_default(1u, 1u);
    bad.radius_near = 0u;
    WS_CHECK(ws_world_host_init(&host, &bad) == WS_ERR_INVALID_ARGUMENT);
    bad = ws_world_host_config_default(1u, 1u);
    bad.radius_load = 1u; /* < radius_mid */
    WS_CHECK(ws_world_host_init(&host, &bad) == WS_ERR_INVALID_ARGUMENT);

    WS_CHECK(ws_world_host_init(&host, &(ws_world_host_config){0}) == WS_ERR_INVALID_ARGUMENT);

    ws_world_host_config cfg = ws_world_host_config_default(1u, 1u);
    WS_CHECK(ws_world_host_init(&host, &cfg) == WS_OK);
    WS_CHECK(ws_world_host_step(&host, 0.0) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_world_host_step(&host, -1.0) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_world_host_step(NULL, 1.0) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_world_host_run(&host, -5.0) == WS_ERR_INVALID_ARGUMENT);

    ws_pop_state s;
    ws_pop_config pc = ws_pop_config_default(1u);
    WS_CHECK(ws_pop_step(NULL, &s, 1.0) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_pop_step(&pc, NULL, 1.0) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_pop_step(&pc, &s, 0.0) == WS_ERR_INVALID_ARGUMENT);
    return 1;
}

int main(void)
{
    const ws_test_case cases[] = {
        {"region geometry and identity", test_region_geometry},
        {"aggregate population bounded", test_population_bounded},
        {"predator-prey coupling", test_population_predator_prey},
        {"loaded set and level assignment", test_host_loaded_set_and_levels},
        {"off-camera progression", test_offcamera_progression},
        {"unload/load without state loss", test_unload_load_no_state_loss},
        {"streaming stays within bounds", test_streaming_bounds},
        {"journal integrity and ordering", test_journal_integrity},
        {"checkpoint save/load roundtrip", test_checkpoint_roundtrip},
        {"replay determinism by digest", test_replay_determinism},
        {"replay across ten seeds", test_replay_across_seeds},
        {"long run stability", test_long_run_stability},
        {"invalid input rejected", test_invalid_input}
    };
    return ws_test_run("world_host", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
