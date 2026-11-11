/* Worldseed Engine — reference quadruped builder.
 *
 * Builds the reference species body (docs/06, docs/01: red deer, Cervus
 * elaphus) as a planar sagittal articulated chain: torso, neck, head, and front
 * and rear legs of two segments each, with muscle-equivalent joint motors and
 * foot contact sensors. All geometry, mass and inertia are derived from the
 * species profile, not from any external asset or animation clip.
 *
 * The profile carries literature-informed proportions and a gait description
 * (stance, stride, duty factor, joint limits, torque budget). Left/right legs
 * are not separate dynamics: the model is planar (sagittal), which is the
 * documented scope for this slice.
 */
#ifndef WS_LIFE_QUADRUPED_H
#define WS_LIFE_QUADRUPED_H

#include <stdint.h>

#include "ws_creature.h"

/* Body indices produced by the builder, so callers can drive the gait without
 * relying on magic numbers. */
typedef struct ws_quad_parts {
    int torso;
    int neck;
    int head;
    int front_upper;
    int front_lower;
    int rear_upper;
    int rear_lower;
} ws_quad_parts;

typedef struct ws_species_profile {
    const char *name;
    double total_mass;   /* kg */
    double body_length;  /* m, nose-to-tail torso+neck span */
    double shoulder_h;   /* m, standing shoulder height */
    double leg_upper;    /* m */
    double leg_lower;    /* m */
    double stance;       /* m, front-to-rear foot separation at rest */
    double gait_freq;    /* Hz, stride frequency at reference speed */
    double duty_factor;  /* fraction of the cycle a foot is in stance */
    double friction;     /* foot-ground Coulomb coefficient */
} ws_species_profile;

/* Literature-informed default for the red deer reference (docs/life model
 * card records sources and validity). */
ws_species_profile ws_species_red_deer(void);

/* Storage the builder needs. Sized for the reference chain (7 bodies, 6 joints,
 * 2 feet); the caller owns it. */
typedef struct ws_quad_storage {
    ws_cbody bodies[8];
    ws_cjoint joints[8];
    ws_cfoot feet[4];
} ws_quad_storage;

/* Build the reference quadruped into `c` (already initialised) using `st`.
 * Places it standing at the origin with the given ground height. Returns WS_OK
 * or an error. */
ws_result ws_quadruped_build(ws_creature *c, ws_quad_storage *st,
                             const ws_species_profile *p, double ground_y,
                             ws_quad_parts *out_parts);

/* Set the gait motor targets for the reference trot at `phase` in [0,1).
 * Front-left/rear-right move together (diagonal pair), as in a trot. Writes
 * each joint's motor_target; the caller owns stepping the phase forward. */
void ws_quadruped_gait_targets(ws_creature *c, const ws_quad_parts *parts,
                               const ws_species_profile *p, double phase);

/* Stateful locomotion controller (Raibert-style foot placement). It advances
 * its own phase by the creature's fixed dt each call and writes joint motor
 * targets so the animal walks forward at `v_des` m/s. `foot_lift` is the swing
 * apex height in metres. The state is caller-owned and zero-initialised before
 * the first call. */
typedef struct ws_gait_state {
    double phase;      /* [0,1) gait phase */
    double td_x[2];    /* front/rear touchdown x, m */
    double lo_x[2];    /* front/rear liftoff x, m */
    int stance[2];     /* front/rear stance flag from the previous step */
    double foot_lift;  /* swing apex, m */
    double kv;         /* velocity-error foot placement gain, s */
    double heading;    /* path heading, rad (yaw); advanced by the turn command */
    double yaw_rate;   /* last commanded yaw rate, rad/s */
    int initialised;
} ws_gait_state;

void ws_gait_state_init(ws_gait_state *g);

/* Initialise the gait state with swing apex and foot-placement gain scaled from
 * the species profile, so a small species does not lift a deer-sized foot or
 * over-correct at its stride scale. The reference deer maps to the same values
 * as ws_gait_state_init. NULL profile falls back to the deer defaults. */
void ws_gait_state_init_for(ws_gait_state *g, const ws_species_profile *p);

void ws_quadruped_walk(ws_creature *c, const ws_quad_parts *parts,
                       const ws_species_profile *p, ws_gait_state *g,
                       double v_des);

/* Walk with a yaw command. The sagittal dynamics produce forward motion; the
 * heading is integrated at `yaw_rate` (rad/s) and is the path-level yaw used by
 * the behaviour layer to steer. Yaw is currently kinematic (the planar slice
 * does not yet generate yaw torque from leg forces); this is recorded in the
 * model card. Returns WS_OK, or an error for invalid input. */
ws_result ws_quadruped_walk_steer(ws_creature *c, const ws_quad_parts *parts,
                                  const ws_species_profile *p, ws_gait_state *g,
                                  double v_des, double yaw_rate);

/* Balance controller: a vestibular reflex torque that holds the torso upright.
 * Applied to body `torso` as a stabiliser torque (not part of the gait). */
void ws_quadruped_balance(ws_creature *c, int torso, double kp, double kd,
                          double target_angle);

/* Default balance gains for a species, scaled from the reference deer by the
 * same mass x length factor the builder uses for the joint torque budget, so a
 * small species is not stabilised with a deer-scale torque. NULL profile gives
 * the reference deer gains. */
void ws_quadruped_default_balance(const ws_species_profile *p,
                                  double *out_kp, double *out_kd);

#endif /* WS_LIFE_QUADRUPED_H */
