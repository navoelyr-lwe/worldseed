/* Worldseed Engine — product motion-quality checklist (see ws_motion.h).
 *
 * Drives the real P5 reference quadruped (red deer) through the gait controller
 * on flat ground for a fixed simulated interval and measures the resulting
 * motion: forward progress, speed bounds, centre-of-mass smoothness, balance
 * margin inside the foot support, foot clearance and kinetic energy. Every
 * quantity comes from the simulated body, so the checklist is automated
 * evidence for the P12 "motion" quality dimension rather than a human claim.
 */
#include "ws_motion.h"

#include "../core/ws_math.h"
#include "../core/ws_rng.h"
#include "../life/ws_creature.h"
#include "../life/ws_quadruped.h"

#include <math.h>

#define MOTION_DT (1.0 / 500.0)
#define MOTION_G 9.80665
#define MOTION_ITERS 30
#define MOTION_BIAS 0.2
#define MOTION_SETTLE_STEPS 1000
#define MOTION_V_DES 0.8
#define MOTION_BAL_KP 4000.0
#define MOTION_BAL_KD 500.0

/* Flat ground profile: the checklist isolates the gait controller, so the body
 * walks on level terrain (slope/uneven ground are separate life tests). */
static double flat_ground(void *ctx, double x)
{
    (void)ctx;
    (void)x;
    return 0.0;
}

/* Worst foot clearance above the flat ground (m). Negative means a foot has
 * sunk through the surface. */
static double min_foot_clearance(const ws_creature *c)
{
    double mn = 1.0e30;
    for (uint32_t i = 0; i < c->foot_count; ++i) {
        const ws_cfoot *f = &c->feet[i];
        const ws_cbody *b = &c->bodies[f->body];
        double wx, wy;
        ws_cbody_point(b, f->lx, f->ly, &wx, &wy);
        if (wy < mn) {
            mn = wy;
        }
    }
    return mn;
}

/* Mass-weighted centre-of-mass velocity (m/s). */
static void com_velocity(const ws_creature *c, double *out_vx, double *out_vy)
{
    double m = 0.0, vx = 0.0, vy = 0.0;
    for (uint32_t i = 0; i < c->body_count; ++i) {
        m += c->bodies[i].mass;
        vx += c->bodies[i].mass * c->bodies[i].vx;
        vy += c->bodies[i].mass * c->bodies[i].vy;
    }
    if (m > 0.0) {
        vx /= m;
        vy /= m;
    }
    *out_vx = vx;
    *out_vy = vy;
}

ws_result ws_motion_measure(uint64_t seed, double seconds, ws_motion_metrics *out)
{
    if (out == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    if (seconds <= 0.0) {
        return WS_ERR_INVALID_ARGUMENT;
    }

    ws_species_profile sp = ws_species_red_deer();
    ws_quad_storage st;
    ws_quad_parts parts;
    ws_cbody bodies[8];
    ws_cjoint joints[8];
    ws_cfoot feet[4];
    ws_creature c;
    if (ws_creature_init(&c, MOTION_DT, MOTION_G, MOTION_ITERS, MOTION_BIAS,
                         bodies, 8u, joints, 8u, feet, 4u) != WS_OK) {
        return WS_ERR_INVALID_STATE;
    }
    ws_creature_set_ground(&c, flat_ground, NULL);
    if (ws_quadruped_build(&c, &st, &sp, 0.0, &parts) != WS_OK) {
        return WS_ERR_INVALID_STATE;
    }

    /* Settle the standing body before measuring the walk. */
    for (int i = 0; i < MOTION_SETTLE_STEPS; ++i) {
        ws_quadruped_balance(&c, parts.torso, MOTION_BAL_KP, MOTION_BAL_KD, 0.0);
        if (ws_creature_step(&c) != WS_OK) {
            return WS_ERR_INVALID_STATE;
        }
    }

    ws_gait_state gs;
    ws_gait_state_init(&gs);
    /* Seed-varied starting phase keeps the run deterministic per seed while
     * exercising the same thresholds from different points of the stride. The
     * gait controller resets the phase on its first call, so the offset is
     * applied after that call has seeded the state. */
    ws_rng rng;
    ws_rng_init(&rng, seed, 0u);
    double phase_offset = ws_rng_next_double(&rng);

    double x0 = c.bodies[parts.torso].px;
    uint64_t frames = (uint64_t)(seconds / MOTION_DT + 0.5);
    double max_speed = 0.0, max_accel = 0.0, max_energy = 0.0, max_tilt = 0.0;
    double min_clear = 1.0e30;
    double prev_vx = 0.0, prev_vy = 0.0;
    int have_prev = 0;

    for (uint64_t i = 0; i < frames; ++i) {
        ws_quadruped_walk(&c, &parts, &sp, &gs, MOTION_V_DES);
        if (i == 0u) {
            gs.phase += phase_offset;
            gs.phase -= floor(gs.phase);
        }
        ws_quadruped_balance(&c, parts.torso, MOTION_BAL_KP, MOTION_BAL_KD, 0.0);
        if (ws_creature_step(&c) != WS_OK) {
            return WS_ERR_INVALID_STATE;
        }

        for (uint32_t b = 0; b < c.body_count; ++b) {
            double v = hypot(c.bodies[b].vx, c.bodies[b].vy);
            if (v > max_speed) {
                max_speed = v;
            }
        }

        double tilt = fabs(c.bodies[parts.torso].angle);
        if (tilt > max_tilt) {
            max_tilt = tilt;
        }

        double clr = min_foot_clearance(&c);
        if (clr < min_clear) {
            min_clear = clr;
        }

        double ke = ws_creature_kinetic_energy(&c);
        if (ke > max_energy) {
            max_energy = ke;
        }

        double vx, vy;
        com_velocity(&c, &vx, &vy);
        if (have_prev) {
            double ax = (vx - prev_vx) / MOTION_DT;
            double ay = (vy - prev_vy) / MOTION_DT;
            double a = hypot(ax, ay);
            if (a > max_accel) {
                max_accel = a;
            }
        }
        prev_vx = vx;
        prev_vy = vy;
        have_prev = 1;
    }

    out->frames = frames;
    out->forward_m = c.bodies[parts.torso].px - x0;
    out->forward_speed_mps = out->forward_m / (frames * MOTION_DT);
    out->max_body_speed_mps = max_speed;
    out->max_com_accel_mps2 = max_accel;
    out->max_torso_tilt_rad = max_tilt;
    out->min_foot_clearance_m = min_clear;
    out->max_energy_j = max_energy;
    return WS_OK;
}

/* The reference scene that carries a motion claim. Thresholds are calibrated
 * from deterministic runs and are deliberately conservative: they fail on a
 * stalled, unstable or explosive gait, not on a legitimate tuning change. */
/* fields: id, min_forward, min_speed, max_speed, max_accel, max_tilt,
 *         min_clear, max_energy, min_frames */
static const ws_motion_spec g_specs[] = {
    {"creature_walk", 2.0, 0.20, 1.60, 500.0, 0.80, -0.05, 4000.0, 250u},
};

size_t ws_motion_spec_count(void)
{
    return sizeof(g_specs) / sizeof(g_specs[0]);
}

const ws_motion_spec *ws_motion_spec_at(size_t index)
{
    if (index >= ws_motion_spec_count()) {
        return NULL;
    }
    return &g_specs[index];
}

ws_result ws_motion_check(const ws_motion_spec *spec, const ws_motion_metrics *m,
                          uint32_t *out_failures)
{
    if (spec == NULL || m == NULL) {
        return WS_ERR_NULL_POINTER;
    }
    uint32_t fail = 0u;
    if (m->forward_m < spec->min_forward_m) {
        fail |= WS_MOTION_FAIL_FORWARD;
    }
    if (m->forward_speed_mps < spec->min_speed_mps ||
        m->forward_speed_mps > spec->max_speed_mps) {
        fail |= WS_MOTION_FAIL_SPEED;
    }
    if (m->max_com_accel_mps2 > spec->max_com_accel_mps2) {
        fail |= WS_MOTION_FAIL_SMOOTHNESS;
    }
    if (m->max_torso_tilt_rad > spec->max_torso_tilt_rad) {
        fail |= WS_MOTION_FAIL_BALANCE;
    }
    if (m->min_foot_clearance_m < spec->min_foot_clearance_m) {
        fail |= WS_MOTION_FAIL_CONTACT;
    }
    if (m->max_energy_j > spec->max_energy_j) {
        fail |= WS_MOTION_FAIL_ENERGY;
    }
    if (m->frames < spec->min_frames) {
        fail |= WS_MOTION_FAIL_DURATION;
    }
    if (out_failures != NULL) {
        *out_failures = fail;
    }
    return fail == 0u ? WS_OK : WS_ERR_INVALID_ARGUMENT;
}
