/* Worldseed Engine — articulated creature body (reference biomechanics).
 *
 * A compact, deterministic, headless model of an articulated body: a small
 * planar (sagittal) chain of rigid links joined by revolute joints, driven by
 * muscle-equivalent joint motors and resting on a terrain profile through foot
 * contacts. It is the foundation for the reference species in docs/06; it is
 * deliberately not a full musculoskeletal solver. Each sub-model's scope and
 * evidence level is recorded in the model cards (docs/life).
 *
 * The reference model is planar (sagittal): forward and vertical motion plus
 * body pitch, with front and rear legs. This captures gait, ground contact,
 * balance and centre-of-mass behaviour at a defensible level of detail; lateral
 * dynamics are a later slice. Left/right legs are rendered for visual
 * plausibility but share the sagittal solution.
 *
 * Dynamics: velocity-level sequential impulse (the same family of method used
 * by the P4 solver). Joints are solved as point-to-point constraints with a
 * Baumgarte position bias, joint angle limits are solved as angular
 * constraints, and feet are solved as normal + Coulomb friction contacts. Joint
 * motors apply a bounded PD torque (muscle equivalent). The fixed step is
 * decoupled from the render frame rate.
 *
 * All quantities are SI: position metres, mass kilograms, angle radians,
 * angular velocity rad/s, torque N*m, force N. Storage is caller-owned so the
 * module allocates nothing on the hot path.
 */
#ifndef WS_LIFE_CREATURE_H
#define WS_LIFE_CREATURE_H

#include <stddef.h>
#include <stdint.h>

#include "../core/ws_math.h"
#include "../core/ws_result.h"

/* Bump when joint, contact or integrator behaviour changes, so a recorded
 * motion is detected as incompatible. */
#define WS_CREATURE_VERSION 1u

/* Upper bound on simultaneous foot contacts; a quadruped has four. */
#define WS_CREATURE_MAX_CONTACTS 8u

/* A rigid link. Angles are about the z axis (planar); local x runs along the
 * link from the joint, local y is the link's thickness. */
typedef struct ws_cbody {
    double mass;        /* kg */
    double inv_mass;    /* 1/kg; 0 for a pinned body */
    double inertia;     /* kg*m^2 about the link centre */
    double inv_inertia; /* 1/inertia; 0 for a pinned body */
    double px;          /* m */
    double py;          /* m */
    double angle;       /* rad */
    double vx;          /* m/s */
    double vy;          /* m/s */
    double av;          /* rad/s */
    double fx;          /* N accumulator */
    double fy;          /* N accumulator */
    double torque;      /* N*m accumulator */
    double extra_torque;/* N*m applied by an external stabiliser, not cleared */
    double half_len;    /* local x half extent, m */
    double half_thick;  /* local y half extent, m */
    double half_width;  /* local z half extent, m (rendering only) */
    uint8_t color[3];   /* sRGB body colour (rendering) */
} ws_cbody;

/* A revolute joint between two bodies. Anchor points are local to each body;
 * the joint keeps them coincident. The joint angle is the relative body angle
 * minus the rest angle, so 0 is the assembly pose. */
typedef struct ws_cjoint {
    int a;             /* parent body index */
    int b;             /* child body index */
    double lax, lay;   /* anchor in a's frame, m */
    double lbx, lby;   /* anchor in b's frame, m */
    double rest_rel;   /* rest (b.angle - a.angle), rad */
    double min_angle;  /* lower limit on joint angle, rad */
    double max_angle;  /* upper limit on joint angle, rad */
    int motor_enabled; /* 1 to drive the joint */
    double motor_target;     /* target joint angle, rad */
    double motor_max_torque; /* |torque| bound, N*m */
    double motor_kp;         /* proportional gain, N*m/rad */
    double motor_kd;         /* derivative gain, N*m*s/rad */

    /* solver scratch (filled each step) */
    double rax, ray, rbx, rby;
    double bias_x, bias_y;
    double k_inv11, k_inv12, k_inv22;
} ws_cjoint;

/* A foot contact sensor: a local point on a body that may touch the ground. */
typedef struct ws_cfoot {
    int body;
    double lx, ly;
    double friction;
} ws_cfoot;

typedef struct ws_ccontact {
    int body;
    double lx, ly;
    double nx, ny;
    double penetration;
    double friction;
    /* solver scratch */
    double rx, ry;
    double kn, kt;
    double lambda_n;
    double lambda_t;
} ws_ccontact;

/* Terrain height profile along the sagittal (x) axis; the normal is derived
 * from the local slope. NULL disables ground contact. */
typedef double (*ws_profile_fn)(void *ctx, double x);

typedef struct ws_creature {
    ws_cbody *bodies;
    uint32_t body_count;
    uint32_t body_capacity;
    ws_cjoint *joints;
    uint32_t joint_count;
    uint32_t joint_capacity;
    ws_cfoot *feet;
    uint32_t foot_count;
    uint32_t foot_capacity;
    ws_ccontact contacts[WS_CREATURE_MAX_CONTACTS];
    uint32_t contact_count;

    double dt;               /* fixed step, s (> 0) */
    double gravity;          /* magnitude, m/s^2 (applied as -y) */
    int solver_iterations;   /* >= 1 */
    double position_bias;    /* Baumgarte factor, 0..1 */

    ws_profile_fn ground;
    void *ground_ctx;

    /* Metabolic-equivalent cost accounting: positive mechanical work done by
     * the joint motors (J) and the mean power over the last step (W). Muscle
     * energy use is approximated by the absolute motor work, a standard
     * positive-work cost proxy; the model card records this limitation. */
    double motor_work;
    double motor_power;
} ws_creature;

/* Fill a body with mass from a solid box (length 2*half_len, thickness
 * 2*half_thick, width 2*half_width) and uniform density; sets mass, inertia and
 * the cached inverses. Caller sets position/angle afterwards. */
void ws_cbody_make_box(ws_cbody *body, double half_len, double half_thick,
                       double half_width, double density, const uint8_t color[3]);

/* Initialise over caller-owned storage. dt > 0, iterations >= 1 and
 * position_bias in [0,1] are required. */
ws_result ws_creature_init(ws_creature *c, double dt, double gravity,
                           int solver_iterations, double position_bias,
                           ws_cbody *bodies, uint32_t body_capacity,
                           ws_cjoint *joints, uint32_t joint_capacity,
                           ws_cfoot *feet, uint32_t foot_capacity);

/* Append a body; returns the index via *out_index when non-NULL. */
ws_result ws_creature_add_body(ws_creature *c, const ws_cbody *body,
                               uint32_t *out_index);

/* Append a joint between two existing bodies. rest_rel is computed from the
 * current body angles. */
ws_result ws_creature_add_joint(ws_creature *c, const ws_cjoint *joint);

/* Append a foot sensor. */
ws_result ws_creature_add_foot(ws_creature *c, const ws_cfoot *foot);

void ws_creature_set_ground(ws_creature *c, ws_profile_fn fn, void *ctx);

/* Advance exactly one fixed step. */
ws_result ws_creature_step(ws_creature *c);

/* Advance `steps` fixed steps. */
ws_result ws_creature_advance(ws_creature *c, uint64_t steps);

/* Total mass (kg), centre of mass (m), and the horizontal support interval
 * spanned by feet currently in contact (out_min/out_max; set equal to the CoM x
 * when there is no contact). */
double ws_creature_total_mass(const ws_creature *c);
void ws_creature_center_of_mass(const ws_creature *c, double *out_x, double *out_y);
int ws_creature_support_interval(const ws_creature *c, double *out_min, double *out_max);

/* Total kinetic energy (J). */
double ws_creature_kinetic_energy(const ws_creature *c);

/* World-space centre of a body's local point. */
void ws_cbody_point(const ws_cbody *b, double lx, double ly,
                    double *out_x, double *out_y);

#endif /* WS_LIFE_CREATURE_H */
