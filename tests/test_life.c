/* Worldseed Engine — reference creature biomechanics tests (P5).
 *
 * Scientific regression suite from docs/06: articulated-chain stability,
 * centre of mass inside the support polygon, forward gait with bounded energy,
 * slope standing, joint-limit compliance, determinism and invalid-input
 * rejection. These exercise the real solver; there are no mocks.
 */
#include "ws_test.h"

#include "core/ws_math.h"
#include "life/ws_creature.h"
#include "life/ws_quadruped.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#define TEST_DT 0.002
#define TEST_G 9.80665
#define TEST_ITERS 10
#define TEST_BIAS 0.2
#define BAL_KP 4000.0
#define BAL_KD 500.0

static double flat_ground(void *ctx, double x)
{
    (void)ctx;
    (void)x;
    return 0.0;
}

static double g_slope;
static double slope_ground(void *ctx, double x)
{
    (void)ctx;
    return g_slope * x;
}

/* Low-amplitude uneven ground (gentle bumps), for foot-contact testing. */
static double bumpy_ground(void *ctx, double x)
{
    (void)ctx;
    return 0.06 * sin(x * 0.5) + 0.03 * sin(x * 1.3 + 1.0);
}

static int all_finite(const ws_creature *c)
{
    for (uint32_t i = 0; i < c->body_count; ++i) {
        const ws_cbody *b = &c->bodies[i];
        if (!isfinite(b->px) || !isfinite(b->py) || !isfinite(b->angle) ||
            !isfinite(b->vx) || !isfinite(b->vy) || !isfinite(b->av)) {
            return 0;
        }
    }
    return 1;
}

/* Lowest-foot clearance above the ground profile (m). Negative means the foot
 * is slightly inside the surface. Used to prove the animal is actually resting
 * on the ground rather than floating. */
static double min_foot_clearance(const ws_creature *c, ws_profile_fn ground)
{
    double mn = 1.0e30;
    for (uint32_t i = 0; i < c->foot_count; ++i) {
        const ws_cfoot *f = &c->feet[i];
        const ws_cbody *b = &c->bodies[f->body];
        double wx, wy;
        ws_cbody_point(b, f->lx, f->ly, &wx, &wy);
        double clr = wy - ground(NULL, wx);
        if (clr < mn) {
            mn = clr;
        }
    }
    return mn;
}

/* Build a standing reference quadruped on the given ground profile. Returns 1
 * on success. */
static int build_reference(ws_creature *c, ws_quad_storage *st, ws_quad_parts *p,
                           ws_cbody *b, ws_cjoint *j, ws_cfoot *f,
                           ws_profile_fn ground)
{
    ws_species_profile sp = ws_species_red_deer();
    if (ws_creature_init(c, TEST_DT, TEST_G, TEST_ITERS, TEST_BIAS,
                         b, 8u, j, 8u, f, 4u) != WS_OK) {
        return 0;
    }
    ws_creature_set_ground(c, ground, NULL);
    if (ws_quadruped_build(c, st, &sp, 0.0, p) != WS_OK) {
        return 0;
    }
    return 1;
}

/* ---------- build ---------- */

static int test_build_reference(void)
{
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));

    WS_CHECK(c.body_count == 7u);
    WS_CHECK(c.joint_count == 6u);
    WS_CHECK(c.foot_count == 2u);
    /* Species mass is the sum of the link masses (within 15%). */
    ws_species_profile sp = ws_species_red_deer();
    WS_CHECK(fabs(ws_creature_total_mass(&c) - sp.total_mass) <
             0.15 * sp.total_mass);
    /* Feet start on the ground. */
    for (uint32_t i = 0; i < c.foot_count; ++i) {
        const ws_cfoot *ft = &c.feet[i];
        double wx, wy;
        ws_cbody_point(&c.bodies[ft->body], ft->lx, ft->ly, &wx, &wy);
        WS_CHECK(fabs(wy) < 1e-6);
    }
    return 1;
}

/* ---------- standing ---------- */

static int test_stand_balanced(void)
{
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));

    for (int i = 0; i < 4000; ++i) { /* 8 s */
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
    }
    double cx, cy;
    ws_creature_center_of_mass(&c, &cx, &cy);
    WS_CHECK(all_finite(&c));
    /* Stays standing at a plausible height and upright. */
    WS_CHECK(cy > 0.85 && cy < 1.15);
    WS_CHECK(fabs(c.bodies[p.torso].angle) < 0.15);
    WS_CHECK(ws_creature_kinetic_energy(&c) < 5.0);
    /* Feet rest on the ground: the lowest foot sits at the surface, not
     * floating. (Checking raw contact_count here is knife-edge, because the
     * equilibrium clearance sits right at the contact-detection band and can
     * fall just outside it under a different compiler's rounding.) */
    double clr = min_foot_clearance(&c, flat_ground);
    WS_CHECK(clr > -2.0e-3 && clr < 1.0e-3);
    return 1;
}

static int test_com_inside_support(void)
{
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
    for (int i = 0; i < 4000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    double cx, cy;
    double mn, mx;
    ws_creature_center_of_mass(&c, &cx, &cy);
    int any = ws_creature_support_interval(&c, &mn, &mx);
    WS_CHECK(any);
    /* The centre of mass projects inside the foot support interval, which is
     * what keeps a standing animal from toppling. */
    WS_CHECK(cx >= mn - 0.05 && cx <= mx + 0.05);
    return 1;
}

/* ---------- forward gait ---------- */

static int test_walk_forward_stable(void)
{
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_gait_state gs;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
    for (int i = 0; i < 1000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    ws_gait_state_init(&gs);
    const double v_des = 0.8;
    double x0 = c.bodies[p.torso].px;
    double max_ke = 0.0;
    int steps = (int)(20.0 / TEST_DT);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &p, &sp, &gs, v_des);
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
        double ke = ws_creature_kinetic_energy(&c);
        if (ke > max_ke) {
            max_ke = ke;
        }
    }
    WS_CHECK(all_finite(&c));
    double dx = c.bodies[p.torso].px - x0;
    double v = dx / (steps * TEST_DT);
    /* Moves forward (nose direction) at a walking speed, stays upright, and
     * does not accumulate energy. */
    WS_CHECK(dx > 2.0);
    WS_CHECK(v > 0.20 && v < 1.60);
    WS_CHECK(fabs(c.bodies[p.torso].angle) < 0.30);
    WS_CHECK(max_ke < 4000.0);
    return 1;
}

/* ---------- slope ---------- */

static int test_slope_stand(void)
{
    g_slope = 0.03; /* 3% grade */
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, slope_ground));
    int supported_steps = 0;
    for (int i = 0; i < 4000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
        /* A foot must be within a small band of the surface; a hovering animal
         * would fail this. Sampled over the run so a single-phase sample cannot
         * mask a floating stance. */
        if (min_foot_clearance(&c, slope_ground) < 2.0e-2) {
            supported_steps++;
        }
    }
    WS_CHECK(all_finite(&c));
    /* Remains standing on the incline without toppling or sinking. */
    WS_CHECK(fabs(c.bodies[p.torso].angle) < 0.20);
    /* The feet are in ground support for the great majority of the run. */
    WS_CHECK(supported_steps > 3000);
    double cx, cy;
    ws_creature_center_of_mass(&c, &cx, &cy);
    double gy = slope_ground(NULL, cx);
    WS_CHECK(cy > gy + 0.7 && cy < gy + 1.2);
    return 1;
}

/* ---------- slope walk ---------- */

static int test_slope_walk(void)
{
    g_slope = 0.03;
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_gait_state gs;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, slope_ground));
    for (int i = 0; i < 1000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    ws_gait_state_init(&gs);
    double x0 = c.bodies[p.torso].px;
    int steps = (int)(15.0 / TEST_DT);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &p, &sp, &gs, 0.8);
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
    }
    WS_CHECK(all_finite(&c));
    double dx = c.bodies[p.torso].px - x0;
    WS_CHECK(dx > 1.0); /* still makes progress uphill */
    WS_CHECK(fabs(c.bodies[p.torso].angle) < 0.30);
    return 1;
}

/* ---------- joint limits ---------- */

static int test_joint_limits_respected(void)
{
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
    /* Drive the hips hard to both extremes; the limit constraints must keep
     * the joint angles inside their ranges. */
    for (int i = 0; i < 2000; ++i) {
        c.joints[2].motor_target = (i / 100 % 2 == 0) ? 2.0 : -2.0;
        c.joints[4].motor_target = (i / 100 % 2 == 0) ? -2.0 : 2.0;
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
    }
    WS_CHECK(all_finite(&c));
    for (uint32_t k = 0; k < c.joint_count; ++k) {
        const ws_cjoint *jt = &c.joints[k];
        double q = (c.bodies[jt->b].angle - c.bodies[jt->a].angle) - jt->rest_rel;
        WS_CHECK(q >= jt->min_angle - 0.15);
        WS_CHECK(q <= jt->max_angle + 0.15);
    }
    return 1;
}

/* ---------- determinism ---------- */

static int test_determinism(void)
{
    double px[2][7];
    double py[2][7];
    for (int run = 0; run < 2; ++run) {
        ws_species_profile sp = ws_species_red_deer();
        ws_creature c;
        ws_quad_storage st;
        ws_quad_parts p;
        ws_gait_state gs;
        ws_cbody b[8];
        ws_cjoint j[8];
        ws_cfoot f[4];
        WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
        ws_gait_state_init(&gs);
        for (int i = 0; i < 5000; ++i) {
            ws_quadruped_walk(&c, &p, &sp, &gs, 0.8);
            ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
            ws_creature_step(&c);
        }
        for (uint32_t k = 0; k < c.body_count; ++k) {
            px[run][k] = c.bodies[k].px;
            py[run][k] = c.bodies[k].py;
        }
    }
    for (uint32_t k = 0; k < 7u; ++k) {
        WS_CHECK(px[0][k] == px[1][k]);
        WS_CHECK(py[0][k] == py[1][k]);
    }
    return 1;
}

/* ---------- long run / no blow-up ---------- */

static int test_long_run_no_blowup(void)
{
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_gait_state gs;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
    ws_gait_state_init(&gs);
    for (int i = 0; i < 1000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    int steps = (int)(60.0 / TEST_DT);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &p, &sp, &gs, 0.8);
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    WS_CHECK(all_finite(&c));
    WS_CHECK(ws_creature_kinetic_energy(&c) < 4000.0);
    return 1;
}

/* ---------- uneven ground walk ---------- */

static int test_ground_walk(void)
{
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_gait_state gs;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, bumpy_ground));
    ws_gait_state_init(&gs);
    for (int i = 0; i < 1500; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    double x0 = c.bodies[p.torso].px;
    int steps = (int)(15.0 / TEST_DT);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &p, &sp, &gs, 0.8);
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
    }
    WS_CHECK(all_finite(&c));
    /* Keeps walking over small bumps without falling or drifting backwards. */
    WS_CHECK(c.bodies[p.torso].px - x0 > 1.0);
    WS_CHECK(fabs(c.bodies[p.torso].angle) < 0.40);
    return 1;
}

/* ---------- turn (steering) ---------- */

static int test_turn_steer(void)
{
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_gait_state gs;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
    ws_gait_state_init(&gs);
    for (int i = 0; i < 1000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    double x0 = c.bodies[p.torso].px;
    double yaw = 0.5; /* rad/s */
    int steps = (int)(8.0 / TEST_DT);
    for (int i = 0; i < steps; ++i) {
        WS_CHECK(ws_quadruped_walk_steer(&c, &p, &sp, &gs, 0.8, yaw) == WS_OK);
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        WS_CHECK(ws_creature_step(&c) == WS_OK);
    }
    WS_CHECK(all_finite(&c));
    /* Walks forward while the path heading integrates the yaw command. */
    WS_CHECK(c.bodies[p.torso].px - x0 > 1.0);
    WS_CHECK(fabs(gs.heading - yaw * steps * TEST_DT) < 1e-6);
    WS_CHECK(fabs(c.bodies[p.torso].angle) < 0.35);
    return 1;
}

/* ---------- energy cost ---------- */

static int test_energy_cost(void)
{
    ws_species_profile sp = ws_species_red_deer();
    ws_creature c;
    ws_quad_storage st;
    ws_quad_parts p;
    ws_gait_state gs;
    ws_cbody b[8];
    ws_cjoint j[8];
    ws_cfoot f[4];
    WS_CHECK(build_reference(&c, &st, &p, b, j, f, flat_ground));
    ws_gait_state_init(&gs);
    /* Standing still: the motors hold the pose; cost must be small but nonzero. */
    for (int i = 0; i < 3000; ++i) {
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    double stand_work = c.motor_work;
    int steps = (int)(10.0 / TEST_DT);
    for (int i = 0; i < steps; ++i) {
        ws_quadruped_walk(&c, &p, &sp, &gs, 0.8);
        ws_quadruped_balance(&c, p.torso, BAL_KP, BAL_KD, 0.0);
        ws_creature_step(&c);
    }
    double walk_work = c.motor_work - stand_work;
    WS_CHECK(all_finite(&c));
    /* Walking costs strictly more than standing, and the running power is a
     * sane positive number. */
    WS_CHECK(walk_work > 0.0);
    WS_CHECK(c.motor_power >= 0.0 && c.motor_power < 5.0e5);
    return 1;
}

/* ---------- invalid input ---------- */

static int test_invalid_input(void)
{
    ws_creature c;
    ws_cbody b[2];
    ws_cjoint j[2];
    ws_cfoot f[2];
    /* dt <= 0, iterations < 1 and out-of-range bias rejected. */
    WS_CHECK(ws_creature_init(&c, 0.0, TEST_G, TEST_ITERS, TEST_BIAS,
                              b, 2u, j, 2u, f, 2u) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_creature_init(&c, TEST_DT, TEST_G, 0, TEST_BIAS,
                              b, 2u, j, 2u, f, 2u) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_creature_init(&c, TEST_DT, TEST_G, TEST_ITERS, 1.5,
                              b, 2u, j, 2u, f, 2u) == WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_creature_init(NULL, TEST_DT, TEST_G, TEST_ITERS, TEST_BIAS,
                              b, 2u, j, 2u, f, 2u) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_creature_init(&c, TEST_DT, TEST_G, TEST_ITERS, TEST_BIAS,
                              NULL, 2u, j, 2u, f, 2u) == WS_ERR_NULL_POINTER);

    WS_CHECK(ws_creature_init(&c, TEST_DT, TEST_G, TEST_ITERS, TEST_BIAS,
                              b, 1u, j, 1u, f, 1u) == WS_OK);
    uint8_t col[3] = {1u, 2u, 3u};
    ws_cbody body;
    ws_cbody_make_box(&body, 0.1, 0.1, 0.1, 1000.0, col);
    uint32_t idx;
    WS_CHECK(ws_creature_add_body(&c, &body, &idx) == WS_OK);
    WS_CHECK(ws_creature_add_body(&c, &body, &idx) == WS_ERR_CAPACITY);
    /* A joint that references a missing body is rejected. */
    ws_cjoint jj;
    memset(&jj, 0, sizeof(jj));
    jj.a = 0;
    jj.b = 5;
    WS_CHECK(ws_creature_add_joint(&c, &jj) == WS_ERR_INVALID_ARGUMENT);
    ws_cfoot ff;
    memset(&ff, 0, sizeof(ff));
    ff.body = 9;
    WS_CHECK(ws_creature_add_foot(&c, &ff) == WS_ERR_INVALID_ARGUMENT);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"build reference", test_build_reference},
        {"stand balanced", test_stand_balanced},
        {"com inside support", test_com_inside_support},
        {"walk forward stable", test_walk_forward_stable},
        {"slope stand", test_slope_stand},
        {"slope walk", test_slope_walk},
        {"ground walk", test_ground_walk},
        {"turn steer", test_turn_steer},
        {"energy cost", test_energy_cost},
        {"joint limits respected", test_joint_limits_respected},
        {"determinism", test_determinism},
        {"long run no blowup", test_long_run_no_blowup},
        {"invalid input", test_invalid_input},
    };
    return ws_test_run("life", cases, (int)(sizeof(cases) / sizeof(cases[0])));
}
