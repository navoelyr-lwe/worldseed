/* Worldseed Engine — product motion-quality checklist.
 *
 * P12 acceptance (docs/13, docs/QUALITY-CHECKLIST.md) requires the product
 * reference scenes to be judged by a checklist covering geometry, material,
 * light, shadow, atmosphere AND motion. Motion cannot be measured from a single
 * still frame, so it is gated separately: this module drives the real reference
 * quadruped (P5 biomechanics) through the gait controller for a fixed simulated
 * interval from a seed-varied start and measures objective, deterministic
 * motion signals of the resulting run.
 *
 * The metrics come from the simulated body itself (positions, velocities,
 * contact support interval, kinetic energy), not from a scripted animation, so
 * they cannot drift from the product. They are technical signals only; the
 * aesthetic reference targets stay in docs/QUALITY-CHECKLIST.md.
 */
#ifndef WS_RENDER_MOTION_H
#define WS_RENDER_MOTION_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_result.h"

/* Objective, deterministic metrics of one driven biomechanical run. */
typedef struct ws_motion_metrics {
    uint64_t frames;             /* fixed simulation steps in the measured run */
    double forward_m;            /* torso forward progress over the run, m */
    double forward_speed_mps;    /* mean forward speed, m/s */
    double max_body_speed_mps;   /* peak speed of any link centre, m/s */
    double max_com_accel_mps2;   /* peak centre-of-mass acceleration, m/s^2 */
    double max_torso_tilt_rad;   /* peak torso pitch from upright, rad (balance) */
    double min_foot_clearance_m; /* worst foot clearance above ground, m
                                  * (negative: foot below the surface) */
    double max_energy_j;         /* peak total kinetic energy, J */
} ws_motion_metrics;

/* Per-scene reference thresholds. A metric equal to its threshold passes. */
typedef struct ws_motion_spec {
    const char *scene_id;        /* reference scene the motion belongs to */
    double min_forward_m;        /* the body must actually travel forward */
    double min_speed_mps;        /* lower bound on mean forward speed */
    double max_speed_mps;        /* upper bound (a walk, not a launch) */
    double max_com_accel_mps2;   /* bounds acceleration spikes (smoothness) */
    double max_torso_tilt_rad;   /* the body must stay near upright (balance) */
    double min_foot_clearance_m; /* feet must not sink through the ground */
    double max_energy_j;         /* rejects energy blow-up */
    uint64_t min_frames;         /* the run must be long enough to be evidence */
} ws_motion_spec;

/* Failure bitmask values reported by ws_motion_check. */
#define WS_MOTION_FAIL_FORWARD    0x01u
#define WS_MOTION_FAIL_SPEED      0x02u
#define WS_MOTION_FAIL_SMOOTHNESS 0x04u
#define WS_MOTION_FAIL_BALANCE    0x08u
#define WS_MOTION_FAIL_CONTACT    0x10u
#define WS_MOTION_FAIL_ENERGY     0x20u
#define WS_MOTION_FAIL_DURATION   0x40u
/* Tool-level: the same seed did not reproduce identical metrics. */
#define WS_MOTION_FAIL_DETERMINISM 0x80u

/* Drive the reference quadruped for a fixed simulated interval from a
 * seed-varied start and measure its motion. Deterministic: the same seed
 * reproduces identical metrics. `seconds` is the simulated interval to measure
 * after the body has settled. */
ws_result ws_motion_measure(uint64_t seed, double seconds, ws_motion_metrics *out);

/* The product reference scenes that carry a motion claim, and their
 * thresholds. Currently the P5 reference quadruped scene (creature_walk). */
size_t ws_motion_spec_count(void);
const ws_motion_spec *ws_motion_spec_at(size_t index);

/* Evaluate metrics against a spec. Returns WS_OK when every check passes;
 * WS_ERR_INVALID_ARGUMENT otherwise, writing the failed-check bitmask to
 * `out_failures` (may be NULL). */
ws_result ws_motion_check(const ws_motion_spec *spec, const ws_motion_metrics *m,
                          uint32_t *out_failures);

#endif /* WS_RENDER_MOTION_H */
