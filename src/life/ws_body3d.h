/* Worldseed Engine — three-dimensional articulated body (P13).
 *
 * The reference creature solver (ws_creature) is a planar (sagittal) chain: it
 * carries forward and vertical motion plus body pitch, with a single left/right
 * leg solution. P13 needs a real third dimension — independent left/right
 * limbs, roll and yaw, and three-dimensional balance. This module is the
 * three-dimensional counterpart of that solver and does not touch it, so the
 * sagittal golden scenes stay bit-identical.
 *
 * Dynamics: the same family as the planar solver (velocity-level sequential
 * impulse with a Baumgarte position bias), lifted to 3D. Orientation is a unit
 * quaternion; each body carries a body-frame inertia tensor (diagonal for a
 * box) and its inverse. Joints are ball-and-socket point constraints solved as
 * a 3x3 effective-mass system, with optional per-axis angular limits and a
 * bounded PD motor. Contacts are point contacts against a height field, solved
 * as a normal impulse plus two tangential Coulomb impulses. The fixed step is
 * decoupled from the render frame rate.
 *
 * All quantities are SI: position metres, mass kilograms, angle radians (and
 * quaternions), angular velocity rad/s, torque N*m, force N. Storage is
 * caller-owned so the module allocates nothing on the hot path.
 */
#ifndef WS_LIFE_BODY3D_H
#define WS_LIFE_BODY3D_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"

/* Bump when joint, contact or integrator behaviour changes, so a recorded
 * motion is detected as incompatible. */
#define WS_BODY3D_VERSION 1u

/* Upper bound on simultaneous foot contacts; a quadruped has four. */
#define WS_BODY3D_MAX_CONTACTS 8u

/* Unit quaternion (w, x, y, z). */
typedef struct ws_quat {
    double w, x, y, z;
} ws_quat;

ws_quat ws_quat_make(double w, double x, double y, double z);
/* Rotate a vector by a unit quaternion: v' = q v q*. */
ws_vec3 ws_quat_rotate(ws_quat q, ws_vec3 v);
/* Hamilton product a*b (apply b, then a). */
ws_quat ws_quat_mul(ws_quat a, ws_quat b);
ws_quat ws_quat_normalize(ws_quat q);
/* Rotation matrix (column-major 3x3) for a unit quaternion. */
ws_mat3 ws_quat_to_mat3(ws_quat q);
/* Integrate orientation by world angular velocity `w` over dt seconds. */
ws_quat ws_quat_integrate(ws_quat q, ws_vec3 w, double dt);

/* A rigid link. Orientation is a unit quaternion; the body-frame inertia tensor
 * is diagonal for a box and stored with its inverse. half_len is the local x
 * half extent, half_thick the local y, half_width the local z. */
typedef struct ws_body3 {
    double mass;             /* kg */
    double inv_mass;         /* 1/kg; 0 for a pinned body */
    ws_mat3 inertia_body;    /* kg*m^2, body frame */
    ws_mat3 inv_inertia_body;/* 1/(kg*m^2), body frame */
    ws_vec3 pos;             /* m, centre of mass, world */
    ws_quat orient;          /* unit quaternion, body -> world */
    ws_vec3 vel;             /* m/s */
    ws_vec3 omega;           /* rad/s, world frame */
    ws_vec3 force;           /* N accumulator */
    ws_vec3 torque;          /* N*m accumulator */
    ws_vec3 extra_torque;    /* N*m applied by an external stabiliser, not cleared */
    double half_len;         /* local x half extent, m */
    double half_thick;       /* local y half extent, m */
    double half_width;       /* local z half extent, m */
    uint8_t color[3];        /* sRGB body colour (rendering) */
} ws_body3;

/* A ball-and-socket joint between two bodies. Anchor points are local to each
 * body; the joint keeps them coincident, which is what gives each limb its own
 * three-dimensional state. An optional motor and one-sided limits act about a
 * single chosen local axis of the parent body (hinge_axis: 0=x, 1=y, 2=z), used
 * to drive the leg's swing; the socket itself stays free in the other two axes. */
typedef struct ws_joint3 {
    int a;                 /* parent body index */
    int b;                 /* child body index */
    ws_vec3 la;            /* anchor in a's frame, m */
    ws_vec3 lb;            /* anchor in b's frame, m */
    int hinge_axis;        /* 0=x, 1=y, 2=z: the motor/limit axis (parent local) */
    int hinge_lock;        /* 1 to constrain the two axes perpendicular to
                            * hinge_axis, making a true hinge (the leg swing);
                            * 0 leaves a free ball socket */
    double rest_angle;     /* hinge angle at assembly, rad (set by add_joint) */
    int limit_enabled;     /* 1 to guard the hinge axis */
    double min_angle;      /* lower limit on the hinge angle, rad */
    double max_angle;      /* upper limit on the hinge angle, rad */
    int motor_enabled;     /* 1 to drive the hinge axis */
    double motor_target;   /* target hinge angle, rad */
    double motor_max_torque; /* |torque| bound, N*m */
    double motor_kp;       /* proportional gain, N*m/rad */
    double motor_kd;       /* derivative gain, N*m*s/rad */
} ws_joint3;

/* A foot contact sensor: a local point on a body that may touch the ground. */
typedef struct ws_foot3 {
    int body;
    ws_vec3 local;         /* local point, m */
    double friction;
} ws_foot3;

typedef struct ws_contact3 {
    int body;
    ws_vec3 local;
    ws_vec3 normal;        /* unit, world */
    double penetration;    /* m, >= 0 */
    double friction;
    /* solver scratch */
    ws_vec3 r;             /* contact offset from CoM, world */
    double kn;
    double lambda_n;
    double lambda_t1;
    double lambda_t2;
} ws_contact3;

/* Terrain height along world x/y; NULL disables ground contact. The normal is
 * derived from the local gradient. */
typedef double (*ws_height_fn)(void *ctx, double x, double y);

typedef struct ws_body3d {
    ws_body3 *bodies;
    uint32_t body_count;
    uint32_t body_capacity;
    ws_joint3 *joints;
    uint32_t joint_count;
    uint32_t joint_capacity;
    ws_foot3 *feet;
    uint32_t foot_count;
    uint32_t foot_capacity;
    ws_contact3 contacts[WS_BODY3D_MAX_CONTACTS];
    uint32_t contact_count;

    double dt;               /* fixed step, s (> 0) */
    double gravity;          /* magnitude, m/s^2 (applied as -y) */
    int solver_iterations;   /* >= 1 */
    double position_bias;    /* Baumgarte factor, 0..1 */

    ws_height_fn ground;
    void *ground_ctx;

    /* Metabolic-equivalent cost accounting: positive mechanical work done by
     * the joint motors (J) and the mean power over the last step (W). */
    double motor_work;
    double motor_power;
} ws_body3d;

/* Fill a body from a solid box (full length 2*half_len, thickness 2*half_thick,
 * width 2*half_width) with uniform density; sets mass, the diagonal inertia
 * tensor and its inverse. Caller sets position/orientation afterwards. */
void ws_body3_make_box(ws_body3 *body, double half_len, double half_thick,
                       double half_width, double density, const uint8_t color[3]);

/* Initialise over caller-owned storage. dt > 0, iterations >= 1 and
 * position_bias in [0,1] are required. */
ws_result ws_body3d_init(ws_body3d *c, double dt, double gravity,
                         int solver_iterations, double position_bias,
                         ws_body3 *bodies, uint32_t body_capacity,
                         ws_joint3 *joints, uint32_t joint_capacity,
                         ws_foot3 *feet, uint32_t foot_capacity);

ws_result ws_body3d_add_body(ws_body3d *c, const ws_body3 *body,
                             uint32_t *out_index);
ws_result ws_body3d_add_joint(ws_body3d *c, const ws_joint3 *joint);
ws_result ws_body3d_add_foot(ws_body3d *c, const ws_foot3 *foot);
void ws_body3d_set_ground(ws_body3d *c, ws_height_fn fn, void *ctx);

/* Advance exactly one fixed step. */
ws_result ws_body3d_step(ws_body3d *c);
/* Advance `steps` fixed steps. */
ws_result ws_body3d_advance(ws_body3d *c, uint64_t steps);

/* World-space centre of a body's local point. */
ws_vec3 ws_body3_point(const ws_body3 *b, ws_vec3 local);
ws_vec3 ws_body3_point_velocity(const ws_body3 *b, ws_vec3 local);
/* World-frame inverse inertia tensor for the body's current orientation. */
ws_mat3 ws_body3_inv_inertia_world(const ws_body3 *b);

/* Current hinge angle of a joint, relative to its assembly pose (rad). */
double ws_joint3_hinge_angle(const ws_body3d *c, const ws_joint3 *j);

double ws_body3d_total_mass(const ws_body3d *c);
ws_vec3 ws_body3d_center_of_mass(const ws_body3d *c);
double ws_body3d_kinetic_energy(const ws_body3d *c);
/* Total linear momentum (kg*m/s). */
ws_vec3 ws_body3d_linear_momentum(const ws_body3d *c);
/* Total angular momentum about the world origin (kg*m^2/s). */
ws_vec3 ws_body3d_angular_momentum(const ws_body3d *c);

#endif /* WS_LIFE_BODY3D_H */
