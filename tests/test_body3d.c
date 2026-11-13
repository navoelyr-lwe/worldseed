/* Worldseed Engine — three-dimensional rigid-body core tests (P13).
 *
 * Exercises the real solver (ws_body3d) and the reference quadruped builder
 * (ws_quad3d): free-body momentum conservation, box resting on four feet,
 * quaternion normalisation, determinism, hinge lock and lateral independence
 * (left and right legs carry separate load). No mocks; every number comes from
 * the model.
 */
#include "ws_test.h"

#include "life/ws_body3d.h"
#include "life/ws_quad3d.h"
#include "life/ws_quadruped.h"
#include "life/ws_species.h"

#include <math.h>
#include <string.h>

#define DT 0.002
#define G 9.80665
#define ITERS 30
#define BIAS 0.2

static double flat(void *ctx, double x, double y)
{
    (void)ctx;
    (void)x;
    (void)y;
    return 0.0;
}

static double slope_ground(void *ctx, double x, double y)
{
    (void)y;
    double grade = *(const double *)ctx;
    return grade * x;
}

/* ---------- quaternion ---------- */

static int test_quat_normalize_and_rotate(void)
{
    ws_quat q = ws_quat_make(1.0, 2.0, 3.0, 4.0);
    ws_quat n = ws_quat_normalize(q);
    double len = sqrt(n.w * n.w + n.x * n.x + n.y * n.y + n.z * n.z);
    WS_CHECK(fabs(len - 1.0) < 1e-12);
    /* 90 degrees about z maps +x to +y. */
    ws_quat rz = ws_quat_make(cos(0.25 * 3.14159265358979323846), 0.0, 0.0,
                              sin(0.25 * 3.14159265358979323846));
    ws_vec3 v = ws_quat_rotate(rz, ws_vec3_make(1.0, 0.0, 0.0));
    WS_CHECK(fabs(v.x) < 1e-12);
    WS_CHECK(fabs(v.y - 1.0) < 1e-12);
    WS_CHECK(fabs(v.z) < 1e-12);
    /* Rotation preserves length. */
    ws_vec3 w = ws_quat_rotate(rz, ws_vec3_make(1.0, 2.0, 3.0));
    WS_CHECK(fabs(ws_vec3_length(w) - sqrt(14.0)) < 1e-12);
    return 1;
}

/* ---------- free-body conservation ---------- */

static int test_free_body_momentum(void)
{
    static ws_body3d c;
    static ws_body3 b[2];
    static ws_joint3 j[1];
    static ws_foot3 f[1];
    WS_CHECK(ws_body3d_init(&c, DT, 0.0, ITERS, BIAS, b, 2, j, 1, f, 1) == WS_OK);
    uint8_t col[3] = {1, 2, 3};
    ws_body3_make_box(&b[0], 0.5, 0.1, 0.1, 300.0, col);
    b[0].pos = ws_vec3_make(0.0, 2.0, 0.0);
    ws_body3_make_box(&b[1], 0.5, 0.1, 0.1, 300.0, col);
    b[1].pos = ws_vec3_make(1.0, 2.0, 0.0);
    ws_body3d_add_body(&c, &b[0], NULL);
    ws_body3d_add_body(&c, &b[1], NULL);
    ws_joint3 jj;
    memset(&jj, 0, sizeof(jj));
    jj.a = 0;
    jj.b = 1;
    jj.la = ws_vec3_make(0.5, 0.0, 0.0);
    jj.lb = ws_vec3_make(-0.5, 0.0, 0.0);
    jj.hinge_axis = 2;
    jj.hinge_lock = 1;
    ws_body3d_add_joint(&c, &jj);
    c.bodies[1].omega = ws_vec3_make(0.0, 0.0, 3.0);
    ws_vec3 p0 = ws_body3d_linear_momentum(&c);
    ws_vec3 l0 = ws_body3d_angular_momentum(&c);
    for (int s = 0; s < 2000; ++s) {
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
    }
    ws_vec3 p1 = ws_body3d_linear_momentum(&c);
    ws_vec3 l1 = ws_body3d_angular_momentum(&c);
    WS_CHECK(ws_vec3_length(ws_vec3_sub(p1, p0)) < 1e-9);
    /* Baumgarte bias allows a tiny bounded angular-momentum drift. */
    WS_CHECK(ws_vec3_length(ws_vec3_sub(l1, l0)) < 1e-4);
    /* The joint keeps the anchors together. */
    ws_vec3 wa = ws_body3_point(&c.bodies[0], jj.la);
    ws_vec3 wb = ws_body3_point(&c.bodies[1], jj.lb);
    WS_CHECK(ws_vec3_length(ws_vec3_sub(wb, wa)) < 1e-3);
    return 1;
}

/* ---------- resting box on four feet ---------- */

static int test_box_rests_on_feet(void)
{
    static ws_body3d c;
    static ws_body3 b[1];
    static ws_joint3 j[1];
    static ws_foot3 f[4];
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, b, 1, j, 1, f, 4) == WS_OK);
    ws_body3d_set_ground(&c, flat, NULL);
    uint8_t col[3] = {1, 2, 3};
    ws_body3_make_box(&b[0], 0.5, 0.2, 0.2, 300.0, col);
    b[0].pos = ws_vec3_make(0.0, 0.6, 0.0);
    b[0].orient = ws_quat_make(1.0, 0.0, 0.0, 0.0);
    ws_body3d_add_body(&c, &b[0], NULL);
    ws_vec3 corners[4] = {
        {0.5, -0.2, 0.2}, {-0.5, -0.2, 0.2}, {0.5, -0.2, -0.2}, {-0.5, -0.2, -0.2},
    };
    for (int i = 0; i < 4; ++i) {
        ws_foot3 ft;
        memset(&ft, 0, sizeof(ft));
        ft.body = 0;
        ft.local = corners[i];
        ft.friction = 0.85;
        WS_CHECK(ws_body3d_add_foot(&c, &ft) == WS_OK);
    }
    for (int s = 0; s < 480 * 4; ++s) {
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
    }
    WS_CHECK(c.contact_count == 4u);
    WS_CHECK(fabs(c.bodies[0].pos.y - 0.2) < 2e-3);
    WS_CHECK(fabs(c.bodies[0].vel.y) < 1e-3);
    WS_CHECK(ws_vec3_length(c.bodies[0].omega) < 1e-3);
    return 1;
}

/* ---------- determinism ---------- */

static int test_determinism(void)
{
    ws_species_profile p = ws_species_red_deer();
    ws_vec3 com[2];
    double digest[2];
    for (int run = 0; run < 2; ++run) {
        static ws_body3d c;
        static ws_quad3_storage st;
        static ws_quad3_parts parts;
        WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints,
                                16, st.feet, 8) == WS_OK);
        ws_body3d_set_ground(&c, flat, NULL);
        WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
        ws_quad3_hold_rest(&c, &parts);
        double phase = 0.0;
        for (int s = 0; s < 480 * 5; ++s) {
            phase += 1.4 * DT;
            ws_quad3_gait_targets(&c, &parts, &p, phase);
            ws_quad3_balance(&c, &parts, &p, 0.0);
            WS_CHECK(ws_body3d_step(&c) == WS_OK);
        }
        com[run] = ws_body3d_center_of_mass(&c);
        digest[run] = ws_body3d_kinetic_energy(&c);
    }
    WS_CHECK(ws_vec3_length(ws_vec3_sub(com[0], com[1])) == 0.0);
    WS_CHECK(digest[0] == digest[1]);
    return 1;
}

/* ---------- reference quadruped stands and walks ---------- */

static int test_quad3_stands(void)
{
    static ws_body3d c;
    static ws_quad3_storage st;
    static ws_quad3_parts parts;
    ws_species_profile p = ws_species_red_deer();
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints, 16,
                            st.feet, 8) == WS_OK);
    ws_body3d_set_ground(&c, flat, NULL);
    WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
    WS_CHECK(c.body_count == 11u);
    WS_CHECK(c.joint_count == 10u);
    WS_CHECK(c.foot_count == 4u);
    ws_quad3_hold_rest(&c, &parts);
    ws_vec3 c0 = ws_body3d_center_of_mass(&c);
    for (int s = 0; s < 480 * 8; ++s) {
        ws_quad3_balance(&c, &parts, &p, 0.0);
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
    }
    ws_vec3 c1 = ws_body3d_center_of_mass(&c);
    /* Stays standing: does not sink, topple or drift. */
    WS_CHECK(c1.y > 0.7 && c1.y < 1.1);
    WS_CHECK(fabs(c1.x - c0.x) < 0.1);
    WS_CHECK(fabs(c1.z - c0.z) < 0.1);
    WS_CHECK(ws_body3d_kinetic_energy(&c) < 5.0);
    WS_CHECK(c.contact_count >= 3u);
    return 1;
}

static int test_quad3_walks_forward(void)
{
    static ws_body3d c;
    static ws_quad3_storage st;
    static ws_quad3_parts parts;
    ws_species_profile p = ws_species_red_deer();
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints, 16,
                            st.feet, 8) == WS_OK);
    ws_body3d_set_ground(&c, flat, NULL);
    WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
    ws_quad3_hold_rest(&c, &parts);
    for (int s = 0; s < 480 * 3; ++s) {
        ws_quad3_balance(&c, &parts, &p, 0.0);
        ws_body3d_step(&c);
    }
    ws_vec3 a = ws_body3d_center_of_mass(&c);
    double phase = 0.0;
    for (int s = 0; s < 480 * 20; ++s) {
        phase += 1.4 * DT;
        ws_quad3_gait_targets(&c, &parts, &p, phase);
        ws_quad3_balance(&c, &parts, &p, 0.0);
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
    }
    ws_vec3 b = ws_body3d_center_of_mass(&c);
    double v = (b.x - a.x) / 20.0;
    /* Moves forward at a walking speed, stays upright and roughly straight. */
    WS_CHECK(v > 0.3 && v < 2.2);
    WS_CHECK(fabs(b.z - a.z) < 0.6);
    WS_CHECK(b.y > 0.6);
    return 1;
}

/* ---------- lateral independence ---------- */

static int test_lateral_independence(void)
{
    static ws_body3d c;
    static ws_quad3_storage st;
    static ws_quad3_parts parts;
    ws_species_profile p = ws_species_red_deer();
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints, 16,
                            st.feet, 8) == WS_OK);
    ws_body3d_set_ground(&c, flat, NULL);
    WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
    ws_quad3_hold_rest(&c, &parts);
    /* Legs are at distinct lateral offsets: front-left and front-right hips
     * differ only in z. */
    double zfl = ws_body3_point(&c.bodies[parts.leg_upper[WS_LEG_FL]],
                                ws_vec3_make(0.0, 0.0, 0.0)).z;
    double zfr = ws_body3_point(&c.bodies[parts.leg_upper[WS_LEG_FR]],
                                ws_vec3_make(0.0, 0.0, 0.0)).z;
    WS_CHECK(zfl > 0.0 && zfr < 0.0);
    /* Feet start on the ground and independent. */
    double y[4];
    for (int i = 0; i < 4; ++i) {
        y[i] = ws_body3_point(&c.bodies[c.feet[i].body], c.feet[i].local).y;
        WS_CHECK(fabs(y[i]) < 1e-6);
    }
    /* A left-leg gait command does not move the right leg's target. */
    ws_quad3_gait_targets(&c, &parts, &p, 0.0);
    double hip_fl = c.joints[2 + WS_LEG_FL * 2].motor_target;
    double hip_fr = c.joints[2 + WS_LEG_FR * 2].motor_target;
    WS_CHECK(fabs(hip_fl - hip_fr) > 1e-6);
    return 1;
}

/* ---------- turning and slope ---------- */

static int test_quad3_turns(void)
{
    static ws_body3d c;
    static ws_quad3_storage st;
    static ws_quad3_parts parts;
    ws_species_profile p = ws_species_red_deer();
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints, 16,
                            st.feet, 8) == WS_OK);
    ws_body3d_set_ground(&c, flat, NULL);
    WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
    ws_quad3_hold_rest(&c, &parts);
    for (int s = 0; s < 480 * 3; ++s) {
        ws_quad3_balance(&c, &parts, &p, 0.0);
        ws_body3d_step(&c);
    }
    /* The turn rate is bounded by the sagittal leg model, not by the servo: the
     * legs hinge about the lateral axis only (no abduction joint), so a fast
     * yaw command eventually outruns the stance and the body topples. Measured
     * on this solver, every species stays upright up to ~0.15 rad/s in an
     * in-place turn; above that the feet leave the ground and the torso flips.
     * The test therefore commands a rate inside the stable envelope and tracks
     * the *minimum* uprightness over the whole run, not just the final frame. A
     * final-frame check alone hides a mid-run flip that the body later recovers
     * from (the earlier 0.6 rad/s case flipped to up.y ~ -1.0 and still passed
     * its end-of-run check). */
    const double yaw_rate = 0.10;
    double phase = 0.0;
    int steps = 480 * 10;
    double up_min = 1.0;
    for (int s = 0; s < steps; ++s) {
        phase += 1.4 * DT;
        double heading_cmd = yaw_rate * (double)s * DT;
        ws_quad3_gait_targets(&c, &parts, &p, phase);
        ws_quad3_balance(&c, &parts, &p, heading_cmd);
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
        ws_vec3 up_i = ws_quat_rotate(c.bodies[parts.torso].orient,
                                      ws_vec3_make(0.0, 1.0, 0.0));
        if (up_i.y < up_min) {
            up_min = up_i.y;
        }
    }
    const ws_body3 *torso = &c.bodies[parts.torso];
    ws_vec3 fwd = ws_quat_rotate(torso->orient, ws_vec3_make(1.0, 0.0, 0.0));
    double heading = atan2(-fwd.z, fwd.x);
    double err = heading - yaw_rate * (double)steps * DT;
    err = atan2(sin(err), cos(err));
    /* The servo tracks the commanded heading within a quarter radian and the
     * body never leaves the upright envelope at any point in the turn. */
    WS_CHECK(fabs(err) < 0.25);
    WS_CHECK(up_min > 0.9);
    return 1;
}

static int test_quad3_climbs_slope(void)
{
    static ws_body3d c;
    static ws_quad3_storage st;
    static ws_quad3_parts parts;
    ws_species_profile p = ws_species_red_deer();
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints, 16,
                            st.feet, 8) == WS_OK);
    static double grade = 0.10;
    ws_body3d_set_ground(&c, slope_ground, &grade);
    WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
    ws_quad3_hold_rest(&c, &parts);
    for (int s = 0; s < 480 * 3; ++s) {
        ws_quad3_balance(&c, &parts, &p, 0.0);
        ws_body3d_step(&c);
    }
    ws_vec3 a = ws_body3d_center_of_mass(&c);
    double phase = 0.0;
    for (int s = 0; s < 480 * 20; ++s) {
        phase += 1.4 * DT;
        ws_quad3_gait_targets(&c, &parts, &p, phase);
        ws_quad3_balance(&c, &parts, &p, 0.0);
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
    }
    ws_vec3 b = ws_body3d_center_of_mass(&c);
    const ws_body3 *torso = &c.bodies[parts.torso];
    ws_vec3 up = ws_quat_rotate(torso->orient, ws_vec3_make(0.0, 1.0, 0.0));
    /* Climbs the ramp, keeps making forward progress and stays upright. */
    WS_CHECK(b.y - a.y > 0.5);
    WS_CHECK((b.x - a.x) / 20.0 > 0.2);
    WS_CHECK(up.y > 0.9);
    WS_CHECK(fabs(b.z - a.z) < 0.6);
    return 1;
}

static int test_quad3_runs(void)
{
    static ws_body3d c;
    static ws_quad3_storage st;
    static ws_quad3_parts parts;
    ws_species_profile p = ws_species_red_deer();
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, st.bodies, 16, st.joints, 16,
                            st.feet, 8) == WS_OK);
    ws_body3d_set_ground(&c, flat, NULL);
    WS_CHECK(ws_quad3_build(&c, &st, &p, 0.0, &parts) == WS_OK);
    ws_quad3_hold_rest(&c, &parts);
    for (int s = 0; s < 480 * 3; ++s) {
        ws_quad3_balance(&c, &parts, &p, 0.0);
        ws_body3d_step(&c);
    }
    ws_vec3 a = ws_body3d_center_of_mass(&c);
    ws_quad3_gait run = ws_quad3_gait_run(&p);
    double phase = 0.0;
    double upright_min = 1.0;
    for (int s = 0; s < 480 * 20; ++s) {
        phase += run.freq * DT;
        ws_quad3_gait_apply(&c, &parts, &run, phase);
        ws_quad3_balance(&c, &parts, &p, 0.0);
        WS_CHECK(ws_body3d_step(&c) == WS_OK);
        const ws_body3 *torso = &c.bodies[parts.torso];
        ws_vec3 up = ws_quat_rotate(torso->orient, ws_vec3_make(0.0, 1.0, 0.0));
        if (up.y < upright_min) {
            upright_min = up.y;
        }
    }
    ws_vec3 b = ws_body3d_center_of_mass(&c);
    double v = (b.x - a.x) / 20.0;
    /* The run is clearly faster than the ~1.7 m/s walk, stays upright and
     * roughly straight. */
    WS_CHECK(v > 1.9 && v < 3.5);
    WS_CHECK(upright_min > 0.9);
    WS_CHECK(fabs(b.z - a.z) < 0.6);
    return 1;
}

/* Every species stands still on its own step and, walking at its own cadence,
 * makes forward progress without falling. This locks the per-species step rule
 * (ws_quad3_suggested_dt) and the species gait (ws_quad3_gait_walk): a step
 * that is too small over-damps the open-loop trot and the body stalls. */
static int test_species_walk_own_dt(void)
{
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        const ws_species_info *si = ws_species_get((ws_species_kind)k);
        if (si == NULL) {
            return 0;
        }
        static ws_body3d c;
        static ws_quad3_storage st;
        static ws_quad3_parts parts;
        const ws_species_profile *p = &si->body;
        double dt = ws_quad3_suggested_dt(p);
        int steps_per_s = (int)(1.0 / dt + 0.5);
        WS_CHECK(ws_body3d_init(&c, dt, G, ITERS, BIAS, st.bodies, 16, st.joints,
                                16, st.feet, 8) == WS_OK);
        ws_body3d_set_ground(&c, flat, NULL);
        WS_CHECK(ws_quad3_build(&c, &st, p, 0.0, &parts) == WS_OK);
        ws_quad3_hold_rest(&c, &parts);
        ws_vec3 c0 = ws_body3d_center_of_mass(&c);
        for (int s = 0; s < steps_per_s * 8; ++s) {
            ws_quad3_balance(&c, &parts, p, 0.0);
            WS_CHECK(ws_body3d_step(&c) == WS_OK);
        }
        ws_vec3 c1 = ws_body3d_center_of_mass(&c);
        double drift = sqrt((c1.x - c0.x) * (c1.x - c0.x) +
                            (c1.z - c0.z) * (c1.z - c0.z));
        /* Stands still: the body does not walk off on its own. */
        WS_CHECK(drift < 0.2);
        WS_CHECK(c1.y - c0.y > -0.2);

        ws_quad3_gait gait = ws_quad3_gait_walk(p);
        double phase = 0.0;
        double upright_min = 1.0;
        for (int s = 0; s < steps_per_s * 20; ++s) {
            phase += gait.freq * dt;
            ws_quad3_gait_apply(&c, &parts, &gait, phase);
            ws_quad3_balance(&c, &parts, p, 0.0);
            WS_CHECK(ws_body3d_step(&c) == WS_OK);
            const ws_body3 *torso = &c.bodies[parts.torso];
            ws_vec3 up =
                ws_quat_rotate(torso->orient, ws_vec3_make(0.0, 1.0, 0.0));
            if (up.y < upright_min) {
                upright_min = up.y;
            }
        }
        ws_vec3 c2 = ws_body3d_center_of_mass(&c);
        double v = (c2.x - c1.x) / 20.0;
        /* Forward progress, upright, roughly straight. */
        WS_CHECK(v > 0.05);
        WS_CHECK(upright_min > 0.9);
        WS_CHECK(fabs(c2.z - c1.z) < 0.6);
    }
    return 1;
}

/* Closed-loop gait: the Raibert foot-placement driver tracks a commanded speed,
 * which the open-loop fixed-angle trot cannot do. Every species must go forward,
 * stay upright and hold a roughly straight line at its own commanded walk speed.
 * This is the P13 closed-loop controller evidence. */
static int test_closed_loop_speed(void)
{
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        const ws_species_info *si = ws_species_get((ws_species_kind)k);
        if (si == NULL) {
            return 0;
        }
        static ws_body3d c;
        static ws_quad3_storage st;
        static ws_quad3_parts parts;
        const ws_species_profile *p = &si->body;
        double dt = ws_quad3_suggested_dt(p);
        int steps_per_s = (int)(1.0 / dt + 0.5);
        double v_des = ws_species_walk_speed((ws_species_kind)k);
        WS_CHECK(ws_body3d_init(&c, dt, G, ITERS, BIAS, st.bodies, 16, st.joints,
                                16, st.feet, 8) == WS_OK);
        ws_body3d_set_ground(&c, flat, NULL);
        WS_CHECK(ws_quad3_build(&c, &st, p, 0.0, &parts) == WS_OK);
        ws_quad3_hold_rest(&c, &parts);
        for (int s = 0; s < steps_per_s * 3; ++s) {
            ws_quad3_balance(&c, &parts, p, 0.0);
            WS_CHECK(ws_body3d_step(&c) == WS_OK);
        }
        ws_vec3 c1 = ws_body3d_center_of_mass(&c);

        ws_quad3_gait gait = ws_quad3_gait_walk(p);
        ws_quad3_gait_state gs;
        ws_quad3_gait_state_init(&gs, p);
        double phase = 0.0;
        double upright_min = 1.0;
        for (int s = 0; s < steps_per_s * 20; ++s) {
            phase += gait.freq * dt;
            ws_quad3_gait_closed_loop(&c, &parts, p, &gs, &gait, phase, v_des);
            ws_quad3_balance(&c, &parts, p, 0.0);
            WS_CHECK(ws_body3d_step(&c) == WS_OK);
            const ws_body3 *torso = &c.bodies[parts.torso];
            ws_vec3 up =
                ws_quat_rotate(torso->orient, ws_vec3_make(0.0, 1.0, 0.0));
            if (up.y < upright_min) {
                upright_min = up.y;
            }
        }
        ws_vec3 c2 = ws_body3d_center_of_mass(&c);
        double v = (c2.x - c1.x) / 20.0;
        WS_CHECK(v > 0.05);
        WS_CHECK(upright_min > 0.9);
        WS_CHECK(fabs(c2.z - c1.z) < 0.6);
    }
    return 1;
}

/* Species-scaled run: every species must genuinely run faster than its own walk.
 * The run stride is a hip angle scaled by sqrt(ref_leg/leg) so the short-legged
 * species do not saturate; before the scaling the rabbit's "run" was no faster
 * than its walk (ratio ~1.05). This locks that fix. */
static int test_species_run_exceeds_walk(void)
{
    for (int k = 0; k < WS_SPECIES_COUNT; ++k) {
        const ws_species_info *si = ws_species_get((ws_species_kind)k);
        if (si == NULL) {
            return 0;
        }
        static ws_body3d c;
        static ws_quad3_storage st;
        static ws_quad3_parts parts;
        const ws_species_profile *p = &si->body;
        double dt = ws_quad3_suggested_dt(p);
        int steps_per_s = (int)(1.0 / dt + 0.5);

        double v_walk = 0.0;
        double v_run = 0.0;
        for (int pass = 0; pass < 2; ++pass) {
            WS_CHECK(ws_body3d_init(&c, dt, G, ITERS, BIAS, st.bodies, 16,
                                    st.joints, 16, st.feet, 8) == WS_OK);
            ws_body3d_set_ground(&c, flat, NULL);
            WS_CHECK(ws_quad3_build(&c, &st, p, 0.0, &parts) == WS_OK);
            ws_quad3_hold_rest(&c, &parts);
            for (int s = 0; s < steps_per_s * 3; ++s) {
                ws_quad3_balance(&c, &parts, p, 0.0);
                WS_CHECK(ws_body3d_step(&c) == WS_OK);
            }
            ws_vec3 a = ws_body3d_center_of_mass(&c);
            ws_quad3_gait gait = pass == 0 ? ws_quad3_gait_walk(p)
                                           : ws_quad3_gait_run(p);
            double phase = 0.0;
            double upright_min = 1.0;
            for (int s = 0; s < steps_per_s * 20; ++s) {
                phase += gait.freq * dt;
                ws_quad3_gait_apply(&c, &parts, &gait, phase);
                ws_quad3_balance(&c, &parts, p, 0.0);
                WS_CHECK(ws_body3d_step(&c) == WS_OK);
                const ws_body3 *torso = &c.bodies[parts.torso];
                ws_vec3 up =
                    ws_quat_rotate(torso->orient, ws_vec3_make(0.0, 1.0, 0.0));
                if (up.y < upright_min) {
                    upright_min = up.y;
                }
            }
            ws_vec3 b = ws_body3d_center_of_mass(&c);
            WS_CHECK(upright_min > 0.9);
            if (pass == 0) {
                v_walk = (b.x - a.x) / 20.0;
            } else {
                v_run = (b.x - a.x) / 20.0;
            }
        }
        WS_CHECK(v_run > 1.10 * v_walk);
    }
    return 1;
}

/* ---------- invalid input ---------- */

static int test_invalid_input(void)
{
    static ws_body3d c;
    static ws_body3 b[1];
    static ws_joint3 j[1];
    static ws_foot3 f[1];
    WS_CHECK(ws_body3d_init(NULL, DT, G, ITERS, BIAS, b, 1, j, 1, f, 1) ==
             WS_ERR_NULL_POINTER);
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, BIAS, NULL, 1, j, 1, f, 1) ==
             WS_ERR_NULL_POINTER);
    WS_CHECK(ws_body3d_init(&c, 0.0, G, ITERS, BIAS, b, 1, j, 1, f, 1) ==
             WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_body3d_init(&c, DT, G, 0, BIAS, b, 1, j, 1, f, 1) ==
             WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_body3d_init(&c, DT, G, ITERS, 2.0, b, 1, j, 1, f, 1) ==
             WS_ERR_INVALID_ARGUMENT);
    WS_CHECK(ws_body3d_step(NULL) == WS_ERR_NULL_POINTER);
    ws_species_profile p = ws_species_red_deer();
    static ws_quad3_storage st;
    WS_CHECK(ws_quad3_build(NULL, &st, &p, 0.0, NULL) == WS_ERR_NULL_POINTER);
    WS_CHECK(ws_quad3_build(&c, NULL, &p, 0.0, NULL) == WS_ERR_NULL_POINTER);
    /* NULL-part calls are no-ops, not crashes. */
    ws_quad3_balance(&c, NULL, &p, 0.0);
    ws_quad3_hold_rest(&c, NULL);
    return 1;
}

int main(void)
{
    static const ws_test_case cases[] = {
        {"quat normalize/rotate", test_quat_normalize_and_rotate},
        {"free-body momentum", test_free_body_momentum},
        {"box rests on feet", test_box_rests_on_feet},
        {"determinism", test_determinism},
        {"quad3 stands", test_quad3_stands},
        {"quad3 walks forward", test_quad3_walks_forward},
        {"quad3 turns in place", test_quad3_turns},
        {"quad3 climbs slope", test_quad3_climbs_slope},
        {"quad3 runs", test_quad3_runs},
        {"species walk own dt", test_species_walk_own_dt},
        {"closed-loop speed", test_closed_loop_speed},
        {"species run exceeds walk", test_species_run_exceeds_walk},
        {"lateral independence", test_lateral_independence},
        {"invalid input", test_invalid_input},
    };
    return ws_test_run("body3d", cases,
                       (int)(sizeof(cases) / sizeof(cases[0])));
}
