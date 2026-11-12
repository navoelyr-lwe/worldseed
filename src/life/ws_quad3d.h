/* Worldseed Engine — three-dimensional reference quadruped builder (P13).
 *
 * The planar builder (ws_quadruped) shares one left/right leg solution, so it
 * cannot show independent limbs, roll or three-dimensional balance. This
 * builder places four separate legs — front-left, front-right, rear-left,
 * rear-right — at distinct lateral offsets on the three-dimensional solver
 * (ws_body3d). Each leg is a two-segment chain on a ball-and-socket hip and
 * knee, so every foot carries its own state and load.
 *
 * Geometry, mass and inertia are derived from the species profile, not from any
 * external asset or animation clip. The profile is the same one the planar
 * builder uses, so a species has one set of proportions across both solvers.
 */
#ifndef WS_LIFE_QUAD3D_H
#define WS_LIFE_QUAD3D_H

#include <stdint.h>

#include "ws_body3d.h"
#include "ws_quadruped.h"

/* Leg order used everywhere: 0=front-left, 1=front-right, 2=rear-left,
 * 3=rear-right. */
typedef enum ws_leg_id {
    WS_LEG_FL = 0,
    WS_LEG_FR = 1,
    WS_LEG_RL = 2,
    WS_LEG_RR = 3,
    WS_LEG_COUNT = 4
} ws_leg_id;

/* Body indices produced by the builder. legs[upper/lower][leg] give the two
 * segments of each leg. */
typedef struct ws_quad3_parts {
    int torso;
    int neck;
    int head;
    int leg_upper[WS_LEG_COUNT];
    int leg_lower[WS_LEG_COUNT];
} ws_quad3_parts;

/* Storage the builder needs (11 bodies, 10 joints, 4 feet); caller owns it. */
typedef struct ws_quad3_storage {
    ws_body3 bodies[16];
    ws_joint3 joints[16];
    ws_foot3 feet[8];
} ws_quad3_storage;

/* Suggested fixed step for a species. Smaller animals have stiffer legs, so the
 * same stable integration needs a smaller step; the reference deer (shoulder
 * 1.05 m) maps to 2 ms and the step scales with the shoulder height to the 3/4
 * power. This keeps every species stable at its own scale. */
double ws_quad3_suggested_dt(const ws_species_profile *p);

/* Build the reference quadruped into `c` (already initialised) using `st`.
 * Places it standing at the origin with the given ground height. Returns WS_OK
 * or an error. */
ws_result ws_quad3_build(ws_body3d *c, ws_quad3_storage *st,
                         const ws_species_profile *p, double ground_y,
                         ws_quad3_parts *out_parts);

/* Write a rest-pose motor target for every leg joint so the legs hold the
 * standing pose (targets equal the assembly angle). */
void ws_quad3_hold_rest(ws_body3d *c, const ws_quad3_parts *parts);

/* A gait description: everything the periodic leg driver needs. Separating it
 * from the species profile lets the same body walk or run (a faster cadence and
 * a longer stride) without changing the profile. */
typedef struct ws_quad3_gait {
    double freq;      /* stride cycles per second */
    double hip_amp;   /* hip swing amplitude (rad) */
    double knee_lift; /* peak knee lift (rad) */
    double duty;      /* stance fraction of the cycle, (0,1) */
} ws_quad3_gait;

/* The species' walk and run gaits. Both derive from the profile's gait
 * frequency and duty factor, so they scale with the species; the run raises the
 * cadence and stride and shortens the stance. */
ws_quad3_gait ws_quad3_gait_walk(const ws_species_profile *p);
ws_quad3_gait ws_quad3_gait_run(const ws_species_profile *p);

/* Drive every leg joint for `phase` in [0,1) using an explicit gait. */
void ws_quad3_gait_apply(ws_body3d *c, const ws_quad3_parts *parts,
                         const ws_quad3_gait *g, double phase);

/* Simple periodic trot: diagonal pairs (front-left with rear-right, front-right
 * with rear-left) swing together. Writes each leg hip/knee motor target for
 * `phase` in [0,1). Knee lift is a species-scaled fraction of leg length. This
 * is the species' walk gait (`ws_quad3_gait_walk`). */
void ws_quad3_gait_targets(ws_body3d *c, const ws_quad3_parts *parts,
                           const ws_species_profile *p, double phase);

/* Vestibular balance reflex: a stabiliser torque on the torso that holds it
 * level (pitch and roll) and drives the world heading (yaw) toward
 * `heading_target` (rad, 0 holds a straight line; a growing target commands a
 * turn). An uncontrolled articulated body falls over and curves away, so this
 * is the 3D counterpart of the planar ws_quadruped_balance. Gains scale with
 * the species mass x length budget. */
void ws_quad3_balance(ws_body3d *c, const ws_quad3_parts *parts,
                      const ws_species_profile *p, double heading_target);

#endif /* WS_LIFE_QUAD3D_H */
